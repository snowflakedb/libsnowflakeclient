/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_RESULTSET_HPP
#define SNOWFLAKECLIENT_RESULTSET_HPP

#include <cstddef>
#include <memory>
#include <string>

#include "cJSON.h"
#include "snowflake/basic_types.h"
#include "snowflake/client.h"

namespace Snowflake
{
namespace Client
{

/**
 * Enumeration over valid query result formats.
 */
enum QueryResultFormat
{
    ARROW,
    JSON
};

/**
 * The implementation of a base result set.
 *
 * We use a partition the results into chunks, where a chunk is simply a maximum
 * unit of memory that may be retrieved and operated on at any given moment.
 */
class ResultSet
{
public:

    /**
     * Default constructor.
     */
    ResultSet();

    /**
     * Parameterized constructor.
     *
     * @param data                      A pointer to the JSON server response data.
     * @param rowset                    A pointer to the JSON array containing result set data.
     * @param metadata                  The metadata of the result set.
     * @param tzString                  The time zone.
     */
    ResultSet(cJSON * data, cJSON * rowset, SF_COLUMN_DESC * metadata, std::string tzString);

    /**
     * Destructor.
     */
    virtual ~ResultSet(){}

    // API methods to populate results =============================================================

    /**
     * Appends the given chunk to the internal result set.
     *
     * @param chunk                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL appendChunk(cJSON * chunk) = 0;

    /**
     * Resets the internal indices so that they may be used to traverse
     * the finished result set for consumption.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL finishResultSet() = 0;

    // API methods to consume results ==============================================================

    /**
     * Advances to the next column or row, depending on the query result format.
     *
     * If Arrow format, advance to the next column.
     * If JSON format, advance to the next row.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL next() = 0;

    /**
     * Writes the value of the given cell as a boolean to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsBool(size_t idx, sf_bool * out_data) = 0;

    /**
     * Writes the value of the given cell as an int8 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsInt8(size_t idx, int8 * out_data) = 0;

    /**
     * Writes the value of the given cell as an int32 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsInt32(size_t idx, int32 * out_data) = 0;

    /**
     * Writes the value of the given cell as an int64 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsInt64(size_t idx, int64 * out_data) = 0;

    /**
     * Writes the value of the given cell as a uint8 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsUint8(size_t idx, uint8 * out_data) = 0;

    /**
     * Writes the value of the given cell as a uint32 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsUint32(size_t idx, uint32 * out_data) = 0;

    /**
     * Writes the value of the given cell as a uint64 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsUint64(size_t idx, uint64 * out_data) = 0;

    /**
     * Writes the value of the given cell as a float32 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsFloat32(size_t idx, float32 * out_data) = 0;

    /**
     * Writes the value of the given cell as a float64 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsFloat64(size_t idx, float64 * out_data) = 0;

    /**
     * Writes the value of the given cell as a constant C-string to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsConstString(size_t idx, const char ** out_data) = 0;

    /**
     * Writes the value of the given cell as a C-string to the provided buffer.
     *
     * In the event that the provided buffer is not large enough to contain the requested string,
     * the buffer will be re-allocated and io_capacity will be updated accordingly.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL
    getCellAsString(size_t idx, char ** out_data, size_t * io_len, size_t * io_capacity) = 0;

    /**
     * Writes the value of the given cell as a timestamp to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data) = 0;

    /**
     * Writes the length of the given cell to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellStrlen(size_t idx, size_t * out_data) = 0;

    /**
     * Indicates whether the given cell is null.
     *
     * @param idx                  The index of the column or row to check is null.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL isCellNull(size_t idx, sf_bool * out_data) = 0;

    // Other member getters ========================================================================

    /**
     * Gets the format of a Binary field.
     *
     * @return The format of a Binary field.
     */
    std::string getBinaryOutputFormat();

    /**
     * Gets the format of a Date field.
     *
     * @return The format of a Date field.
     */
    std::string getDateOutputFormat();

    /**
     * Gets the format of a Time field.
     *
     * @return The format of a Time field.
     */
    std::string getTimeOutputFormat();

    /**
     * Gets the format of a Timestamp field.
     *
     * @return The format of a Timestamp field.
     */
    std::string getTimestampOutputFormat();

    /**
     * Gets the format of a Timestamp LTZ (local time zone) field.
     *
     * @return The format of a Timestamp LTZ field.
     */
    std::string getTimestampLtzOutputFormat();

    /**
     * Gets the format of a Timestamp NTZ (no time zone) field.
     *
     * @return The format of a Timestamp NTZ field.
     */
    std::string getTimestampNtzOutputFormat();

    /**
     * Gets the format of a Timestamp TZ (time zone) field.
     *
     * @return The format of a Timestamp TZ field.
     */
    std::string getTimestampTzOutputFormat();

    /**
     * Gets the format of a column in the result set.
     *
     * @return The format of a column in the result set.
     */
    QueryResultFormat getQueryResultFormat();

    /**
     * Gets the tz string to use when dealing with time or timestamp values.
     *
     * @return The tz string to use when dealing with time or timestamp values.
     */
    std::string getTzString();

    /**
     * Gets the tz offset to use when dealing with time or timestamp values.
     *
     * @return The tz offset to use when dealing with time or timestamp values.
     */
    int32 getTzOffset();

    /**
     * Gets the total number of chunks that the result set is divided into.
     *
     * @return The total number of chunks that the result set is divided into.
     */
    size_t getTotalChunkCount();

    /**
     * Gets the total number of columns in the result set.
     *
     * @return The total number of columns in the result set.
     */
    size_t getTotalColumnCount();

    /**
     * Gets the total number of rows in the result set.
     *
     * @return The total number of rows in the result set.
     */
    size_t getTotalRowCount();

protected:

    // Helper methods ==============================================================================

    /**
     * Converts the given time zone offset into a time zone string.
     *
     * This string will have the format: ±HH:MM.
     *
     * Note: The time offset will never be negative, even for negative offsets.
     * Instead, UTC+0 is stored as 24*60=1440. Thus, values lie in [0, 2880].
     */
    void initTzString();

    // Protected members ===========================================================================

    /**
     * The metadata object.
     */
    SF_COLUMN_DESC * m_metadata;

    /**
     * The output format of a Binary field.
     */
    std::string m_binaryOutputFormat;

    /**
     * The output format of a Date field.
     */
    std::string m_dateOutputFormat;

    /**
     * The output format of a Time field.
     */
    std::string m_timeOutputFormat;

    /**
     * The output format of a Timestamp field.
     */
    std::string m_timestampOutputFormat;

    /**
     * The output format of a Timestamp LTZ (local time zone) field.
     */
    std::string m_timestampLtzOutputFormat;

    /**
     * The output format of a Timestamp NTZ (no time zone) field.
     */
    std::string m_timestampNtzOutputFormat;

    /**
     * The output format of a Timestamp TZ (time zone) field.
     */
    std::string m_timestampTzOutputFormat;

    /**
     * Indicates whether the query that generated this result set is a DML statement.
     */
    bool m_isDml;

    /**
     * The format of the result set.
     */
    QueryResultFormat m_queryResultFormat;

    /**
     * The tz string to use when dealing with time or timestamp values.
     */
    std::string m_tzString;

    /**
     * The tz offset to use when dealing with time or timestamp values.
     */
    int32 m_tzOffset;

    /**
     * The index of the current chunk.
     */
    size_t m_currChunkIdx;

    /**
     * The index of the current row in the current chunk.
     */
    size_t m_currChunkRowIdx;

    /**
     * The index of the current column.
     */
    size_t m_currColumnIdx;

    /**
     * The index of the current row.
     */
    size_t m_currRowIdx;

    /**
     * The total number of chunks.
     */
    size_t m_totalChunkCount;

    /**
     * The total number of columns.
     */
    size_t m_totalColumnCount;

    /**
     * The total number of rows.
     */
    size_t m_totalRowCount;
};

}
}

#endif // SNOWFLAKECLIENT_RESULTSET_HPP
