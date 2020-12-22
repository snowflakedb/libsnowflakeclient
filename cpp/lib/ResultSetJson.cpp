/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/ResultSetJson.hpp"

namespace Snowflake
{
namespace Client
{


Snowflake::Client::ResultSetJson::ResultSetJson() :
    Snowflake::Client::ResultSet()
{
    m_columnFormat = ColumnFormat::JSON;
}

Snowflake::Client::ResultSetJson::ResultSetJson(
    cJSON * data,
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
    totalRowCount)
{
    m_columnFormat = ColumnFormat::JSON;
    init();
}

Snowflake::Client::ResultSetJson::~ResultSetJson()
{
    snowflake_cJSON_Delete(m_records);
    chunk_downloader_term(m_chunkDownloader);
}

bool Snowflake::Client::ResultSetJson::nextRow()
{
    // If all rows have been exhausted, then we can't advance further.
    if (m_currRowIdx >= m_totalRowCount)
    {
        return false;
    }

    m_currRowIdx++;
    return true;
}

cJSON * Snowflake::Client::ResultSetJson::getCurrentRow()
{
    return snowflake_cJSON_GetArrayItem(m_records, m_currRowIdx);
}

std::string Snowflake::Client::ResultSetJson::getColumnAsString(size_t columnIdx)
{
    return "";
}

// Private methods

void Snowflake::Client::ResultSetJson::init()
{
    // Populate data until all rows in the chunk downloader are consumed.
    do
    {
        cJSON * currRow = snowflake_cJSON_GetArrayItem(
            m_chunkDownloader.queue[m_currChunkIdx].chunk,
            m_currChunkRowIdx);
        snowflake_cJSON_AddItemToArray(m_records, currRow);
    }
    while (advance());

    // Reset row indices so that they can be re-used by public API.
    m_currChunkIdx = 0;
    m_currChunkRowIdx = 0;
    m_currRowIdx = 0;
}

bool Snowflake::Client::ResultSetJson::advance()
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
