/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_RESULTSET_HPP
#define SNOWFLAKECLIENT_RESULTSET_HPP

#include <cstddef>
#include <memory>
#include <string>

#include "basic_types.h"
#include "chunk_downloader.h"

namespace Snowflake
{
namespace Client
{

/**
 * Enumeration over valid column formats.
 */
enum ColumnFormat
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
     * @param chunkDownloader           A pointer to the chunk downloader object.
     * @param queryId                   The ID of the query.
     * @param tzOffset                  The time zone offset.
     * @param totalChunkCount           The total number of chunks in the result set.
     * @param totalColumnCount          The total number of columns in the result set.
     * @param totalRowCount             The total number of rows in the result set.
     */
    ResultSet(
        SF_CHUNK_DOWNLOADER * chunkDownloader,
        std::string queryId,
        int32 tzOffset,
        size_t totalChunkCount,
        size_t totalColumnCount,
        size_t totalRowCount
    );

    /**
     * Destructor.
     */
    virtual ~ResultSet(){}

    /**
     * Advance to the next row, moving over to the next chunk if necessary.
     *
     * @return true if next row exists, false if next row does not exist.
     */
    virtual bool nextRow() = 0;

    /**
     * Get the format of a Binary field.
     *
     * @return The format of a Binary field.
     */
    std::string getBinaryOutputFormat();

    /**
     * Get the format of a Date field.
     *
     * @return The format of a Date field.
     */
    std::string getDateOutputFormat();

    /**
     * Get the format of a Time field.
     *
     * @return The format of a Time field.
     */
    std::string getTimeOutputFormat();

    /**
     * Get the format of a Timestamp field.
     *
     * @return The format of a Timestamp field.
     */
    std::string getTimestampOutputFormat();

    /**
     * Get the format of a Timestamp LTZ (local time zone) field.
     *
     * @return The format of a Timestamp LTZ field.
     */
    std::string getTimestampLtzOutputFormat();

    /**
     * Get the format of a Timestamp NTZ (no time zone) field.
     *
     * @return The format of a Timestamp NTZ field.
     */
    std::string getTimestampNtzOutputFormat();

    /**
     * Get the format of a Timestamp TZ (time zone) field.
     *
     * @return The format of a Timestamp TZ field.
     */
    std::string getTimestampTzOutputFormat();

    /**
     * Get the format of a column in the result set.
     *
     * @return The format of a column in the result set.
     */
    ColumnFormat getColumnFormat();

    /**
     * Get the tz string to use when dealing with time or timestamp values.
     *
     * @return The tz string to use when dealing with time or timestamp values.
     */
    std::string getTzString();

    /**
     * Get the tz offset to use when dealing with time or timestamp values.
     *
     * @return The tz offset to use when dealing with time or timestamp values.
     */
    int32 getTzOffset();

    /**
     * Get the total number of chunks that the result set is divided into.
     *
     * @return The total number of chunks that the result set is divided into.
     */
    size_t getTotalChunkCount();

    /**
     * Get the total number of columns in the result set.
     *
     * @return The total number of columns in the result set.
     */
    size_t getTotalColumnCount();

    /**
     * Get the total number of rows in the result set.
     *
     * @return The total number of rows in the result set.
     */
    size_t getTotalRowCount();

    /**
     * Indicates whether bulk fetch is enabled or not.
     *
     * @return true if bulk fetch is enabled.
     */
    bool isBulkFetchEnabled();

protected:
    /**
     * Converts the given time zone offset into a time zone string.
     *
     * This string will have the format: ±HH:MM.
     *
     * Note: The time offset will never be negative, even for negative offsets.
     * Instead, UTC+0 is stored as 24*60=1440. Thus, values lie in [0, 2880].
     */
    void initTzString();

    /**
     * The chunk downloader object.
     */
    SF_CHUNK_DOWNLOADER * m_chunkDownloader;
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
     * The ID of the query.
     */
    std::string m_queryId;

    /**
     * The format of a column in the result set.
     */
    ColumnFormat m_columnFormat;

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

    /**
     * A flag indicating whether bulk fetching is enabled or not.
     * By default, bulk fetching is disabled.
     */
    bool m_isBulkFetchEnabled;
};

}
}

#endif // SNOWFLAKECLIENT_RESULTSET_HPP
