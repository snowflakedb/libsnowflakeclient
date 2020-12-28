/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <time.h>

#include "snowflake/ResultSetArrow.hpp"
#include "results.h"

namespace
{
    /**
     * Helper method to determine the length of the given integer.
     * Does not count signs toward the length.
     *
     * @param i                    The integer whose length to calculate.
     *
     * @return The length of the given integer.
     */
    inline std::size_t intlen(int32 i)
    {
        if (i == 0) return 1;
        else if (i < 0) return 1 + static_cast<std::size_t>(std::log10(-i));
        else if (i > 0) return 1 + static_cast<std::size_t>(std::log10(i));
    }
}

namespace Snowflake
{
namespace Client
{


Snowflake::Client::ResultSetArrow::ResultSetArrow() :
    Snowflake::Client::ResultSet()
{
    m_columnFormat = ColumnFormat::ARROW;
}

Snowflake::Client::ResultSetArrow::ResultSetArrow(
    cJSON * data,
    SF_COLUMN_DESC * metadata,
    SF_CHUNK_DOWNLOADER * chunkDownloader,
    std::string queryId,
    int32 tzOffset,
    size_t totalChunkCount,
    size_t totalColumnCount,
    size_t totalRowCount
) : Snowflake::Client::ResultSet(
    chunkDownloader,
    queryId,
    tzOffset,
    totalChunkCount,
    totalColumnCount,
    totalRowCount
)
{
    m_columnFormat = ColumnFormat::ARROW;
    init(data);
}

Snowflake::Client::ResultSetArrow::~ResultSetArrow()
{
    chunk_downloader_term(m_chunkDownloader);
}

std::string Snowflake::Client::ResultSetArrow::getCurrentColumnAsString()
{
    return m_records->column(m_currColumnIdx)->ToString();
}

bool Snowflake::Client::ResultSetArrow::nextColumn()
{
    if (m_currColumnIdx >= m_totalColumnCount)
    {
        return false;
    }

    m_currColumnIdx++;
    return true;
}

bool Snowflake::Client::ResultSetArrow::nextRow()
{
    if (m_currRowIdx >= m_totalRowCount)
    {
        return false;
    }

    m_currRowIdx++;
    return true;
}

// Private methods

void Snowflake::Client::ResultSetArrow::init(cJSON * data)
{
    std::shared_ptr<arrow::Schema> schema = initSchema();
    std::vector<std::shared_ptr<arrow::Array>> columns = initColumns(data);
    m_records = arrow::RecordBatch::Make(schema, m_totalRowCount, columns);

    // Reset row indices so that they can be re-used by public API.
    m_currChunkIdx = 0;
    m_currChunkRowIdx = 0;
    m_currRowIdx = 0;

    // Finally, clean up the chunk downloader resource.
    delete m_chunkDownloader;
}

std::shared_ptr<arrow::Schema> Snowflake::Client::ResultSetArrow::initSchema()
{
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::shared_ptr<arrow::Schema> schema;

    // Iterate over the column metadata and populate the schema with appropriate
    // Arrow fields.
    while (m_currColumnIdx < m_totalColumnCount)
    {
        SF_DB_TYPE currSnowflakeType = m_metadata[m_currColumnIdx].type;
        std::string currName(m_metadata[m_currColumnIdx].name);
        int64 currScale = m_metadata[m_currColumnIdx].scale;
        fields.push_back(createArrowField(
            currSnowflakeType,
            currName,
            currScale));
        m_currColumnIdx++;
    }

    schema = arrow::schema(fields);

    // Reset column index for future use.
    m_currColumnIdx = 0;

    return schema;
}

std::vector<std::shared_ptr<arrow::Array>>
Snowflake::Client::ResultSetArrow::initColumns(cJSON * data)
{
    std::vector<std::shared_ptr<arrow::Array>> columns;
    std::vector<arrow::ArrayBuilder *> builders = initBuilders(data);

    for (arrow::ArrayBuilder * currBuilder : builders)
    {
        std::shared_ptr<arrow::Array> currColumn;

        if (!currBuilder->Finish(&currColumn).ok())
        {
            // Error!
        }

        columns.push_back(currColumn);
    }

    return columns;
}

std::vector<arrow::ArrayBuilder *>
Snowflake::Client::ResultSetArrow::initBuilders(cJSON * data)
{
    // Iterate over the column metadata and initialize array builders for each column.
    std::vector<arrow::ArrayBuilder *> builders;
    while (m_currColumnIdx < m_totalColumnCount)
    {
        SF_DB_TYPE currSnowflakeType = m_metadata[m_currColumnIdx].type;
        int64 scale = m_metadata[m_currColumnIdx].scale;
        builders.push_back(createArrowArrayBuilder(currSnowflakeType, scale));
        m_currColumnIdx++;
    }

    // Reset column index for future use.
    m_currColumnIdx = 0;

    // Populate builders with partial results already retrieved in first chunk.
    appendChunkToBuilders(builders, data);

    // Populate builders with any remaining chunks until all rows are consumed.
    do
    {
        appendChunkToBuilders(
            builders,
            m_chunkDownloader->queue[m_currChunkIdx].chunk);
    }
    while (advance());

    return builders;
}

std::shared_ptr<arrow::Field>
Snowflake::Client::ResultSetArrow::createArrowField(
    SF_DB_TYPE snowflakeType,
    std::string name,
    int64 scale
)
{
    std::shared_ptr<arrow::DataType> arrowType =
        convertTypeFromSnowflakeToArrow(snowflakeType, scale);

    return arrow::field(name, arrowType);
}

arrow::ArrayBuilder *
Snowflake::Client::ResultSetArrow::createArrowArrayBuilder(
    SF_DB_TYPE snowflakeType,
    int64 scale
)
{
    switch (snowflakeType)
    {
        case SF_DB_TYPE_BINARY:
        {
            arrow::StringBuilder * builder =
                new arrow::StringBuilder(arrow::default_memory_pool());
            // Since each column should contain exactly as many elements as there are
            // rows in the result set, resize the array builder for performance.
            builder->Resize(m_totalRowCount);
            return builder;
        }
        case SF_DB_TYPE_BOOLEAN:
        {
            arrow::BooleanBuilder * builder =
                new arrow::BooleanBuilder(arrow::default_memory_pool());
            builder->Resize(m_totalRowCount);
            return builder;
        }
        case SF_DB_TYPE_FIXED:
        {
            if (scale == 0)
            {
                arrow::Int64Builder * builder =
                    new arrow::Int64Builder(arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            else
            {
                arrow::DoubleBuilder * builder =
                    new arrow::DoubleBuilder(arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
        }
        case SF_DB_TYPE_REAL:
        {
            arrow::DoubleBuilder * builder =
                new arrow::DoubleBuilder(arrow::default_memory_pool());
            builder->Resize(m_totalRowCount);
            return builder;
        }
        case SF_DB_TYPE_DATE:
        {
            arrow::Date32Builder * builder =
                new arrow::Date32Builder(arrow::default_memory_pool());
            builder->Resize(m_totalRowCount);
            return builder;
        }
        case SF_DB_TYPE_TIME:
        {
            if (scale == 0)
            {
                arrow::Time32Builder * builder = new arrow::Time32Builder(
                    arrow::time32(arrow::TimeUnit::SECOND),
                    arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            else if (scale <= 3)
            {
                arrow::Time32Builder * builder = new arrow::Time32Builder(
                    arrow::time32(arrow::TimeUnit::MILLI),
                    arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            else if (scale <= 6)
            {
                arrow::Time64Builder * builder = new arrow::Time64Builder(
                    arrow::time64(arrow::TimeUnit::MICRO),
                    arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            else
            {
                arrow::Time64Builder * builder = new arrow::Time64Builder(
                    arrow::time64(arrow::TimeUnit::NANO),
                    arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            break;
        }
        case SF_DB_TYPE_TIMESTAMP_LTZ:
        case SF_DB_TYPE_TIMESTAMP_NTZ:
        case SF_DB_TYPE_TIMESTAMP_TZ:
        {
            if (scale == 0)
            {
                arrow::TimestampBuilder * builder = new arrow::TimestampBuilder(
                    arrow::timestamp(arrow::TimeUnit::SECOND),
                    arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            else if (scale <= 3)
            {
                arrow::TimestampBuilder * builder = new arrow::TimestampBuilder(
                    arrow::timestamp(arrow::TimeUnit::MILLI),
                    arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            else if (scale <= 6)
            {
                arrow::TimestampBuilder * builder = new arrow::TimestampBuilder(
                    arrow::timestamp(arrow::TimeUnit::MICRO),
                    arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            else
            {
                arrow::TimestampBuilder * builder = new arrow::TimestampBuilder(
                    arrow::timestamp(arrow::TimeUnit::NANO),
                    arrow::default_memory_pool());
                builder->Resize(m_totalRowCount);
                return builder;
            }
            break; 
        }
        case SF_DB_TYPE_TEXT:
        case SF_DB_TYPE_ANY:
        case SF_DB_TYPE_ARRAY:
        case SF_DB_TYPE_OBJECT:
        case SF_DB_TYPE_VARIANT:
        default:
        {
            arrow::StringBuilder * builder =
                new arrow::StringBuilder(arrow::default_memory_pool());
            builder->Resize(m_totalRowCount);
            return builder;
        }
    }
}

void Snowflake::Client::ResultSetArrow::appendChunkToBuilders(
    std::vector<arrow::ArrayBuilder *> builders,
    cJSON * chunk)
{
    // Iterate over the rows in the chunk as well as the cells in each row.
    // Convert the values of each cell to Arrow format, referencing the metadata
    // to obtain the appropriate type, and append one by one to the builders.
    while (m_currChunkRowIdx < snowflake_cJSON_GetArraySize(chunk))
    {
        cJSON * currJsonRow =
            snowflake_cJSON_GetArrayItem(chunk, m_currChunkRowIdx);

        // 1. From the Snowflake DB type, determine the C type and Arrow type.
        // 2. Cast the JSON value to an appropriate C value.
        // 3. Append the C value to the Arrow builder.
        while (m_currColumnIdx < m_totalColumnCount)
        {
            cJSON * currJsonValue =
                snowflake_cJSON_GetArrayItem(currJsonRow, m_currColumnIdx);

            convertAndAppendValue(currJsonValue, builders);
            m_currColumnIdx++;
        }

        m_currColumnIdx = 0;
        m_currChunkRowIdx++;
        m_currRowIdx++;
    }

    // Reset chunk row index for future use.
    m_currChunkRowIdx = 0;
}

void Snowflake::Client::ResultSetArrow::convertAndAppendValue(
    cJSON * jsonValue,
    std::vector<arrow::ArrayBuilder *> builders)
{
    if (!jsonValue)
    {
        return;
    }

    SF_DB_TYPE snowflakeType = m_metadata[m_currColumnIdx].type;
    int64 precision = m_metadata[m_currColumnIdx].precision;
    int64 scale = m_metadata[m_currColumnIdx].scale;

    if (convertTypeFromSnowflakeToArrow(snowflakeType, scale)
        != builders[m_currColumnIdx]->type())
    {
        // Error!
    }

    SF_C_TYPE cType = snowflake_to_c_type(snowflakeType, precision, scale);

    // cJSON stores numeric types as a double or int.
    // All others data types are stored as a C-style string.
    switch (cType)
    {
        case SF_C_TYPE_NULL:
        {
            arrow::NullBuilder * b =
                dynamic_cast<arrow::NullBuilder *>(builders[m_currColumnIdx]);
            b->AppendNull();
            break;
        }
        case SF_C_TYPE_BINARY:
        {
            if (m_binaryOutputFormat == "HEX")
            {
                std::string hexValue(jsonValue->valuestring);
                dynamic_cast<arrow::StringBuilder *>(builders[m_currColumnIdx])
                    ->Append(hexValue);
            }
            else if (m_binaryOutputFormat == "BASE64")
            {
                // TODO: Implement this.
            }
            break;
        }
        case SF_C_TYPE_BOOLEAN:
        {
            if (snowflake_cJSON_IsTrue)
                dynamic_cast<arrow::BooleanBuilder *>(builders[m_currColumnIdx])
                    ->UnsafeAppend(true);
            else
                dynamic_cast<arrow::BooleanBuilder *>(builders[m_currColumnIdx])
                    ->UnsafeAppend(false);
            break;
        }
        case SF_C_TYPE_INT8:
        {
            int8 i8Value = static_cast<int8>(jsonValue->valueint);
            dynamic_cast<arrow::Int8Builder *>(builders[m_currColumnIdx])
                ->UnsafeAppend(i8Value);
            break;
        }
        case SF_C_TYPE_INT64:
        {
            int64 i64Value = static_cast<int64>(jsonValue->valueint);
            dynamic_cast<arrow::Int64Builder *>(builders[m_currColumnIdx])
                ->UnsafeAppend(i64Value);
            break;
        }
        case SF_C_TYPE_UINT8:
        {
            uint8 u8Value = static_cast<uint8>(jsonValue->valueint);
            dynamic_cast<arrow::UInt8Builder *>(builders[m_currColumnIdx])
                ->UnsafeAppend(u8Value);
            break;
        }
        case SF_C_TYPE_UINT64:
        {
            uint64 u64Value = static_cast<uint64>(jsonValue->valueint);
            dynamic_cast<arrow::UInt64Builder *>(builders[m_currColumnIdx])
                ->UnsafeAppend(u64Value);
            break;
        }
        case SF_C_TYPE_FLOAT64:
        {
            // Already a float64 type. No conversion necessary.
            dynamic_cast<arrow::DoubleBuilder *>(builders[m_currColumnIdx])
                ->UnsafeAppend(jsonValue->valuedouble);
            break;
        }
        case SF_C_TYPE_TIMESTAMP:
        {
            ArrowTimestamp tsValue =
                convertTimestampStringToStruct(jsonValue->valuestring);

            // If working with fractional seconds, combine the fractional
            // component with the seconds since Epoch value.
            if (scale == 0)
            {
                dynamic_cast<arrow::TimestampBuilder *>(builders[m_currColumnIdx])
                    ->UnsafeAppend(tsValue.secondsSinceEpoch);
            }
            else
            {
                // Round up to nearest multiple of 3.
                int32 exp = (scale % 3 == 0) ? scale : scale + (3 - scale % 3);
                size_t len = ::intlen(tsValue.fracSeconds);
                int32 frac = std::pow(10, exp - len) * tsValue.fracSeconds;
                frac += std::pow(10, exp) * tsValue.secondsSinceEpoch;
                dynamic_cast<arrow::TimestampBuilder *>(builders[m_currColumnIdx])
                    ->UnsafeAppend(frac);
            }
            break;
        }
        case SF_C_TYPE_STRING:
        {
            // String types currently handle many cases.
            // Since Date and Time types end up as different types,
            // handle them with custom logic.
            char * strValue = jsonValue->valuestring;

            if (snowflakeType == SF_DB_TYPE_DATE)
            {
                // Date32 stores days since Epoch.
                // By default, Date values are usually stored as YYYY-MM-DD.
                int32 dateValue = convertDateStringToInt32(strValue);
                dynamic_cast<arrow::Date32Builder *>(builders[m_currColumnIdx])
                    ->UnsafeAppend(dateValue);
            }
            else if (snowflakeType == SF_DB_TYPE_TIME)
            {
                // Time32 stores up to milliseconds since midnight.
                // Time64 stores up to nanoseconds since midnight.
                // By default, Time values are stored as HH:MM:SS.NNNNNNNNN.
                if (scale <= 3)
                {
                    int32 timeValue = convertTimeStringToInt32(strValue, scale);
                    dynamic_cast<arrow::Time32Builder *>(builders[m_currColumnIdx])
                        ->UnsafeAppend(timeValue);
                }
                else
                {
                    int64 timeValue = convertTimeStringToInt64(strValue, scale);
                    dynamic_cast<arrow::Time64Builder *>(builders[m_currColumnIdx])
                        ->UnsafeAppend(timeValue);
                }
            }
            else
            {
                // No more special cases. Handle as a default string value.
                dynamic_cast<arrow::StringBuilder *>(builders[m_currColumnIdx])
                    ->Append(std::string(strValue));
            }
            break;
        }
        default:
        {
            char * value = jsonValue->valuestring;
            dynamic_cast<arrow::StringBuilder *>(builders[m_currColumnIdx])
                ->Append(std::string(value));
            break;
        }
    }
}

std::shared_ptr<arrow::DataType>
Snowflake::Client::ResultSetArrow::convertTypeFromSnowflakeToArrow(
    SF_DB_TYPE snowflakeType,
    int64 scale
)
{
    std::shared_ptr<arrow::DataType> dt;

    switch (snowflakeType)
    {
        case SF_DB_TYPE_BINARY:
        {
            dt = arrow::binary();
            break;
        }
        case SF_DB_TYPE_BOOLEAN:
        {
            dt = arrow::boolean();
            break;
        }
        case SF_DB_TYPE_FIXED:
        {
            if (scale > 0)
                dt = arrow::float64();
            else
                dt = arrow::int64();
            break;
        }
        case SF_DB_TYPE_REAL:
        {
            dt = arrow::float64();
            break;
        }
        case SF_DB_TYPE_DATE:
        {
            dt = arrow::date32();
            break;
        }
        case SF_DB_TYPE_TIME:
        {
            // time32() and time64() require a time unit to be passed.
            if (scale == 0)
                dt = arrow::time32(arrow::TimeUnit::SECOND);
            else if (scale <= 3)
                dt = arrow::time32(arrow::TimeUnit::MILLI);
            else if (scale <= 6)
                dt = arrow::time64(arrow::TimeUnit::MICRO);
            else
                dt = arrow::time64(arrow::TimeUnit::NANO);
            break;
        }
        case SF_DB_TYPE_TIMESTAMP_LTZ:
        case SF_DB_TYPE_TIMESTAMP_NTZ:
        case SF_DB_TYPE_TIMESTAMP_TZ:
        {
            // timestamp() requires a time unit to be passed.
            // Additionally pass the time zone name.
            if (scale == 0)
                dt = arrow::timestamp(arrow::TimeUnit::SECOND, m_tzString);
            else if (scale <= 3)
                dt = arrow::timestamp(arrow::TimeUnit::MILLI, m_tzString);
            else if (scale <= 6)
                dt = arrow::timestamp(arrow::TimeUnit::MICRO, m_tzString);
            else
                dt = arrow::timestamp(arrow::TimeUnit::NANO, m_tzString);
            break;
        }
        case SF_DB_TYPE_TEXT:
        case SF_DB_TYPE_ANY:
        case SF_DB_TYPE_ARRAY:
        case SF_DB_TYPE_OBJECT:
        case SF_DB_TYPE_VARIANT:
        default:
        {
            // Default to string type.
            dt = arrow::utf8();
            break;
        }
    }

    return dt;
}

int32 Snowflake::Client::ResultSetArrow::convertDateStringToInt32(std::string ds)
{
    std::tm t = {0};
    std::istringstream ss(ds);
    ss >> std::get_time(&t, m_dateOutputFormat.c_str());
    int64 secondsSinceEpoch = mktime(&t);
    return secondsSinceEpoch / (24 * 60 * 60);
}

int32 Snowflake::Client::ResultSetArrow::convertTimeStringToInt32(
    std::string ts,
    int64 scale
)
{
    // Round scale up to nearest multiple of 3.
    int32 exp = (scale == 0) ? 0 : 3;
    int32 c = std::pow(10, exp);

    // s will come in the format: "HH:MM:SS{.FFF}"
    int32 hh = std::stoi(ts.substr(0, 2));
    int32 mm = std::stoi(ts.substr(3, 2));
    int32 ss = std::stoi(ts.substr(6, 2));
    int32 fff = ts.find('.') == std::string::npos ? 0 : std::stoi(ts.substr(9));
    int32 frac = std::pow(10, exp - ::intlen(fff)) * fff;

    return (24 * 60 * c * hh) + (60 * c * mm) + (c * ss) + frac;
}

int64 Snowflake::Client::ResultSetArrow::convertTimeStringToInt64(
    std::string ts,
    int64 scale
)
{
    // Round scale up to nearest multiple of 3.
    int64 exp = (scale % 3 == 0) ? scale : scale + (3 - scale % 3);
    int64 c = std::pow(10, exp);

    // s will come in the format: "HH:MM:SS.FFFF{FFFFF}"
    int64 hh = std::stoi(ts.substr(0, 2));
    int64 mm = std::stoi(ts.substr(3, 2));
    int64 ss = std::stoi(ts.substr(6, 2));
    int64 fff = std::stoi(ts.substr(9));
    int64 frac = std::pow(10, exp - ::intlen(fff)) * fff;

    return (24 * 60 * c * hh) + (60 * c * mm) + (c * ss) + frac;
}

ArrowTimestamp
Snowflake::Client::ResultSetArrow::convertTimestampStringToStruct(
    char * tsCString
)
{
    ArrowTimestamp ts;
    ts.fracSeconds = 0;
    ts.timeOffset = 0;

    // Timestamp string syntax:
    //   "{seconds_since_epoch}.{fractional_seconds} {time_offset}"
    // where the time offset field is optional.
    //
    // The time offset will never be negative, even for negative time offsets.
    // Instead, UTC+0 is stored as 24*60=1440. Thus, values lie in [0, 2880].
    //
    // Examples:
    //   "2014-05-03 13:56:46.123  +09:00" => "1399093006.12300 1980"
    //   "1969-11-21 05:17:23.0123 -01:00" => "-3519756.98770 1380"
    std::string tsString(tsCString);
    size_t dotIdx = tsString.find('.');
    size_t spaceIdx = tsString.find(' ');

    if (dotIdx != std::string::npos && spaceIdx != std::string::npos)
    {
        ts.secondsSinceEpoch = std::stoll(tsString.substr(0, dotIdx));
        ts.fracSeconds = std::stoi(tsString.substr(dotIdx, spaceIdx));
        ts.timeOffset = std::stoi(tsString.substr(spaceIdx, std::string::npos));
    }
    else if (dotIdx == std::string::npos && spaceIdx != std::string::npos)
    {
        ts.secondsSinceEpoch = std::stoll(tsString.substr(0, spaceIdx));
        ts.fracSeconds = std::stoi(tsString.substr(dotIdx, std::string::npos));
    }
    else
    {
        ts.secondsSinceEpoch = std::stoll(tsString);
    }

    return ts;
}

bool Snowflake::Client::ResultSetArrow::advance()
{
    // If all rows have been exhausted, then we can't advance further.
    if (m_currRowIdx >= m_totalRowCount)
    {
        return false;
    }

    // Advance within the current chunk if there are rows left to consume.
    // Otherwise, reset the chunk-row index and advance to the next chunk.
    if (m_currChunkRowIdx >= m_chunkDownloader->queue[m_currChunkIdx].row_count)
    {
        m_currChunkRowIdx = 0;
        m_currChunkIdx++;
    }
    else
    {
        ++m_currChunkRowIdx;
    }

    // Finally, update the chunk downloader's consumer_head value.
    m_chunkDownloader->consumer_head++;
    m_currRowIdx++;

    return true;
}

}
}
