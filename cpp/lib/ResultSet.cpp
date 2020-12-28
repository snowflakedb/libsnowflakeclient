/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#include <cstdio>

#include "snowflake/ResultSet.hpp"
#include "snowflake/Simba_CRTFunctionSafe.h"

namespace Snowflake
{
namespace Client
{

Snowflake::Client::ResultSet::ResultSet() :
    m_binaryOutputFormat("HEX"),
    m_dateOutputFormat("YYYY-MM-DD"),
    m_timeOutputFormat("HH24:MI:SS"),
    m_timestampOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampLtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampNtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3"),
    m_timestampTzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_isBulkFetchEnabled(false),
    m_currChunkIdx(0),
    m_currChunkRowIdx(0),
    m_currColumnIdx(0),
    m_currRowIdx(0)
{
    ;
}

Snowflake::Client::ResultSet::ResultSet(
    SF_CHUNK_DOWNLOADER * chunkDownloader,
    std::string queryId,
    int32 tzOffset,
    size_t totalChunkCount,
    size_t totalColumnCount,
    size_t totalRowCount
) : m_binaryOutputFormat("HEX"),
    m_dateOutputFormat("YYYY-MM-DD"),
    m_timeOutputFormat("HH24:MI:SS"),
    m_timestampOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampLtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampNtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3"),
    m_timestampTzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_isBulkFetchEnabled(false),
    m_tzOffset(tzOffset),
    m_chunkDownloader(chunkDownloader),
    m_currChunkIdx(0),
    m_currChunkRowIdx(0),
    m_currColumnIdx(0),
    m_currRowIdx(0),
    m_queryId(queryId),
    m_totalChunkCount(totalChunkCount),
    m_totalColumnCount(totalColumnCount),
    m_totalRowCount(totalRowCount)
{
    initTzString();
}

std::string Snowflake::Client::ResultSet::getBinaryOutputFormat()
{
    return m_binaryOutputFormat;
}

std::string Snowflake::Client::ResultSet::getDateOutputFormat()
{
    return m_dateOutputFormat;
}

std::string Snowflake::Client::ResultSet::getTimeOutputFormat()
{
    return m_timeOutputFormat;
}

std::string Snowflake::Client::ResultSet::getTimestampOutputFormat()
{
    return m_timestampOutputFormat;
}

std::string Snowflake::Client::ResultSet::getTimestampLtzOutputFormat()
{
    return m_timestampLtzOutputFormat;
}

std::string Snowflake::Client::ResultSet::getTimestampNtzOutputFormat()
{
    return m_timestampNtzOutputFormat;
}

std::string Snowflake::Client::ResultSet::getTimestampTzOutputFormat()
{
    return m_timestampTzOutputFormat;
}

Snowflake::Client::ColumnFormat Snowflake::Client::ResultSet::getColumnFormat()
{
    return m_columnFormat;
}

size_t Snowflake::Client::ResultSet::getTotalChunkCount()
{
    return m_totalChunkCount;
}

size_t Snowflake::Client::ResultSet::getTotalColumnCount()
{
    return m_totalColumnCount;
}

size_t Snowflake::Client::ResultSet::getTotalRowCount()
{
    return m_totalRowCount;
}

bool Snowflake::Client::ResultSet::isBulkFetchEnabled()
{
    return m_isBulkFetchEnabled;
}

// Private methods

void Snowflake::Client::ResultSet::initTzString()
{
    int zeroOffset = 1440;
    bool isPositiveOffset = zeroOffset >= m_tzOffset;

    char signChar = isPositiveOffset ? '+' : '-';

    // Extract HH and MM values from time offset.
    int absOffset =
        isPositiveOffset ? m_tzOffset - zeroOffset : zeroOffset - m_tzOffset;
    int hh = (int) absOffset / 60;
    int mm = absOffset % 60;

    char buffer[6];
    sb_sprintf(buffer, 6, "%c%02d:%02d", signChar, hh, mm);

    m_tzString = std::string(buffer);
}

}
}
