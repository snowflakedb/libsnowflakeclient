/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#include "ResultSetArrow.hpp"
#include <memory>

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
    init();
}

Snowflake::Client::ResultSetArrow::~ResultSetArrow()
{
    chunk_downloader_term(m_chunkDownloader);
}

std::string Snowflake::Client::ResultSetArrow::getCurrentColumnAsString()
{
    return m_records.column(m_currColumnIdx).toString();
}

std::string Snowflake::Client::ResultSetArrow::getCurrentRowAsString()
{
    return m_records.Slice(m_currRowIdx, 1).toString();
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
    // If all rows have been exhausted, then we can't advance further.
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
    std::shared_ptr<ArrayColumn> columns = initColumns(data);
    m_records = arrow::RecordBatch.Make(schema, m_totalRowCount, columns);

    // Reset row indices so that they can be re-used by public API.
    m_currChunkIdx = 0;
    m_currChunkRowIdx = 0;
    m_currRowIdx = 0;

    // Finally, clean up the chunk downloader resource.
    delete m_chunkDownloader;
}

std::shared_ptr<arrow::Schema> Snowflake::Client::ResultSetArrow::initSchema()
{
    std::shared_ptr<arrow::Field> fields;
    std::shared_ptr<arrow::Schema> schema;

    // Iterate over the column metadata and populate the schema with appropriate Arrow fields.
    while (m_currColumnIdx < m_totalColumnCount)
    {
        SF_DB_TYPE currSnowflakeType = m_metadata[m_currColumnIdx].type;
        fields.push_back(getArrowField(currSnowflakeType));
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
    std::vector<std::shared_ptr<arrow::ArrayBuilder>> builders = initBuilders();

    for (currBuilder : builders)
    {
        std::shared_ptr<arrow::Array> currColumn;

        if (!currBuilder.Finish(&currColumn).ok())
        {
            // Error!
        }

        columns.push_back(currColumn);
    }

    return columns;
}

std::vector<std::shared_ptr<arrow::ArrayBuilder>>
Snowflake::Client::ResultSetArrow::initBuilders(cJSON * data)
{
    // Iterate over the column metadata and initialize array builders for each column.
    std::vector<std::shared_ptr<arrow::ArrayBuilder>> builders;
    while (m_currColumnIdx < m_totalColumnCount)
    {
        SF_DB_TYPE currSnowflakeType = m_metadata[m_currColumnIdx].type;
        builders.push_back(createArrayBuilder(currSnowflakeType));
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
            m_chunkDownloader.queue[m_currChunkIdx].chunk);
    }
    while (advance());

    return builders;
}

arrow::ArrayBuilder
Snowflake::Client::ResultSetArrow::createArrayBuilder(
    SF_DB_TYPE snowflakeType,
    int64 scale
)
{
    arrow::ArrayBuilder builder;

    switch (snowflakeType)
    {
        case SF_DB_TYPE_BINARY:
            builder = arrow::BinaryBuilder();
            break;
        case SF_DB_TYPE_BOOLEAN:
            builder = arrow::BooleanBuilder();
            break;
        case SF_DB_TYPE_FIXED:
            if (scale > 0)
                builder = arrow::Float64Builder();
            else
                builder = arrow::Int64Builder();
            break;
        case SF_DB_TYPE_REAL:
            builder = arrow::Float64Builder();
            break;
        case SF_DB_TYPE_TIME:
            builder = arrow::Time64Builder();
            break;
        case SF_DB_TYPE_DATE:
            builder = arrow::Date64Builder();
            break;
        case SF_DB_TYPE_TIMESTAMP_LTZ:
        case SF_DB_TYPE_TIMESTAMP_NTZ:
        case SF_DB_TYPE_TIMESTAMP_TZ:
            // The Timestamp builder is one of the few that takes arguments.
            // Construct it with an appropriate time unit and time zone.
            arrow::TimeUnit tu;

            if (scale == 0)
                tu = arrow::TimeUnit::SECOND;
            else if (scale <= 3)
                tu = arrow::TimeUnit::MILLI;
            else if (scale <= 6)
                tu = arrow::TimeUnit::MICRO;
            else
                tu = arrow::TimeUnit::NANO;

            builder = arrow::TimestampBuilder(tu, m_tzString);
            break; 
        case SF_DB_TYPE_TEXT:
        case SF_DB_TYPE_ANY:
        case SF_DB_TYPE_ARRAY:
        case SF_DB_TYPE_OBJECT:
        case SF_DB_TYPE_VARIANT:
        default:
            builder = arrow::StringBuilder();
            break;
    }

    // Since each column should contain exactly as many elements as there are
    // rows in the result set, resize the array builder for performance.
    builder.Resize(m_totalRowCount);

    return builder;
}

Snowflake::Client::ResultSetArrow::appendChunkToBuilders(
    std::vector<std::shared_ptr<arrow::ArrayBuilder>> builders,
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
                snowflake_cJSON_GetArrayItem(currJsonRow);

            convertAndAppendValue(currJsonValue, m_currColumnIdx);
            m_currColumnIdx++;
        }

        m_currColumnIdx = 0;
        m_currChunkRowIdx++;
        m_currRowIdx++;
    }

    // Reset chunk row index for future use.
    m_currChunkRowIdx = 0;
}

void Snowflake::Client::ResultSetArrow::convertAndAppendValue(cJSON * jsonValue)
{
    if (!jsonValue)
    {
        return NULL;
    }

    if (convertTypeFromSnowflakeToArrow(currSnowflakeType)
        != builders[m_currColumnIdx].type)
    {
        // Error!
    }

    SF_C_TYPE cType = snowflake_to_c_type(
        m_metadata[m_currColumnIdx].type,
        m_metadata[m_currColumnIdx].precision,
        m_metadata[m_currColumnIdx].scale);

    // cJSON stores numeric types as a double or int.
    // All others data types are stored as a C-style string.
    switch (cType)
    {
        case SF_C_TYPE_NULL:
            builders[m_currColumnIdx].AppendNull();
            break;
        case SF_C_TYPE_BINARY:
            break;
        case SF_C_TYPE_BOOLEAN:
            if (snowflake_cJSON_IsTrue)
                builders[m_currColumnIdx].UnsafeAppend(true);
            else
                builders[m_currColumnIdx].UnsafeAppend(false);
            break;
        case SF_C_TYPE_INT8:
            int8 value = static_cast<int8> jsonValue->valueint;
            builders[m_currColumnIdx].UnsafeAppend(value);
            break;
        case SF_C_TYPE_INT64:
            uint8 value = static_cast<int64> jsonValue->valueint;
            builders[m_currColumnIdx].UnsafeAppend(value);
            break;
        case SF_C_TYPE_UINT8:
            uint64 value = static_cast<uint8> jsonValue->valueint;
            builders[m_currColumnIdx].UnsafeAppend(value);
            break;
        case SF_C_TYPE_UINT64:
            uint64 value = static_cast<uint64> jsonValue->valueint;
            builders[m_currColumnIdx].UnsafeAppend(value);
            break;
        case SF_C_TYPE_FLOAT64:
            float64 value = jsonValue->valuedouble;
            builders[m_currColumnIdx].UnsafeAppend(value);
            break;
        case SF_C_TYPE_TIMESTAMP:
            ArrowTimestamp value =
                convertToArrowTimestamp(jsonValue->valuestring);

            if ()
            builders[m_currColumnIdx].UnsafeAppend(
                value.secondsSinceEpoch,
                value.fracSeconds);
            break;
        case SF_C_TYPE_STRING:
        default:
            char * value = jsonValue->valuestring;
            builders[m_currColumnIdx].UnsafeAppend(value);
            break;
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
            dt = arrow::binary();
            break;
        case SF_DB_TYPE_BOOLEAN:
            dt = arrow::boolean();
            break;
        case SF_DB_TYPE_FIXED:
            if (scale > 0)
                dt = arrow::float64();
            else
                dt = arrow::int64();
            break;
        case SF_DB_TYPE_REAL:
            dt = arrow::float64();
            break;
        case SF_DB_TYPE_TIME:
            dt = arrow::time64();
            break;
        case SF_DB_TYPE_DATE:
            dt = arrow::Date64Type();
            break;
        case SF_DB_TYPE_TIMESTAMP_LTZ:
        case SF_DB_TYPE_TIMESTAMP_NTZ:
        case SF_DB_TYPE_TIMESTAMP_TZ:
            dt = arrow::TimestampType();
            break;
        case SF_DB_TYPE_TEXT:
        case SF_DB_TYPE_ANY:
        case SF_DB_TYPE_ARRAY:
        case SF_DB_TYPE_OBJECT:
        case SF_DB_TYPE_VARIANT:
        default:
            // Default to string type.
            dt = arrow::utf8();
            break;
    }

    return dt;
}

ArrowTimestamp
Snowflake::Client::ResultSetArrow::convertToArrowTimestamp(char * tsCString)
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
        std::stoll(tsString);
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
