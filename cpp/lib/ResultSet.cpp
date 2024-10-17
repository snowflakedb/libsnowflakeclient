/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "snowflake/SF_CRTFunctionSafe.h"
#include "DataConversion.hpp"
#include "ResultSet.hpp"

namespace Snowflake
{
namespace Client
{

ResultSet::ResultSet(QueryResultFormat format) :
    m_binaryOutputFormat("HEX"),
    m_dateOutputFormat("YYYY-MM-DD"),
    m_timeOutputFormat("HH24:MI:SS"),
    m_timestampOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampLtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampNtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3"),
    m_timestampTzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_currChunkIdx(0),
    m_currChunkRowIdx(0),
    m_currColumnIdx(0),
    m_currRowIdx(0),
    m_queryResultFormat(format)
{
    ;
}

ResultSet::ResultSet(
    SF_COLUMN_DESC * metadata,
    std::string tzString,
    QueryResultFormat format
) :
    m_currChunkIdx(0),
    m_currChunkRowIdx(0),
    m_currColumnIdx(0),
    m_currRowIdx(0),
    m_totalChunkCount(0),
    m_totalColumnCount(0),
    m_metadata(metadata),
    m_queryResultFormat(format),
    m_isFirstChunk(true),
    m_tzString(tzString),
    m_error(SF_STATUS_SUCCESS)
{
    ;
}

} // namespace Client
} // namespace Snowflake
