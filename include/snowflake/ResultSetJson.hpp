/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_RESULTSETJSON_HPP
#define SNOWFLAKECLIENT_RESULTSETJSON_HPP

#include "snowflake/ResultSet.hpp"
#include "cJSON.h"

namespace Snowflake
{
namespace Client
{

class ResultSetJson : public Snowflake::Client::ResultSet
{
public:

    /**
     * Default constructor.
     */
    ResultSetJson();

    /**
     * Parameterized constructor.
     *
     * This class takes ownership of the "data" and "chunkDownloader" pointers.
     * Upon invoking the destructor, these resources will be deleted.
     *
     * @param data                      A pointer to the JSON array containing result set data.
     * @param chunkDownloader           A pointer to the chunk downloader object.
     * @param queryId                   The query ID.
     * @param tzOffset                  The time zone offset.
     * @param totalChunkCount           The total number of chunks in the result set.
     * @param totalColumnCount          The total number of columns in the result set.
     * @param totalRowCount             The total number of rows in the result set.
     */
    ResultSetJson(
        cJSON * data,
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
    ~ResultSetJson();

    /**
     * Advance to the next row.
     *
     * @return true if next row exists, otherwise false.
     */
    bool nextRow();

    /**
     * Get the data in the current row as a cJSON struct.
     *
     * @return The current row as a cJSON struct.
     */
    cJSON * getCurrentRow();

    /**
     * Get the value of the given column as a string.
     *
     * @param columnIdx
     *
     * @return The value of the given column as a string.
     */
    std::string getColumnAsString(size_t columnIdx);

private:

    /**
     * Helper method to initialize the result set data.
     *
     * Populates m_records by using m_chunkDownloader to eagerly fetch rows.
     */
    void init();

    /**
     * Helper method to advance index values while initializing rowset data.
     *
     * @return true if next row exists, otherwise false.
     */
    bool advance();

    /**
     * The JSON array representing the result set.
     */
    cJSON * m_records;
};

}
}

#endif // SNOWFLAKECLIENT_RESULTSETJSON_HPP
