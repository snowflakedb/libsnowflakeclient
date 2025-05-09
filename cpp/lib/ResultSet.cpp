#include <cstdio>
#include <cstring>
#include <ctime>

#include "../logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "snowflake/SF_CRTFunctionSafe.h"
#include "DataConversion.hpp"
#include "ResultSet.hpp"
#include "ResultSetJson.hpp"
#include "ResultSetArrow.hpp"

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
    const std::string& tzString,
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

ResultSet* ResultSet::CreateResultFromJson(cJSON* json_rowset,
                                           SF_COLUMN_DESC* metadata,
                                           QueryResultFormat query_result_format,
                                           const std::string& tz_string)
{
  switch (query_result_format)
  {
#ifndef SF_WIN32
  case SF_ARROW_FORMAT:
    return new Snowflake::Client::ResultSetArrow(json_rowset, metadata, tz_string);
#endif
  case SF_JSON_FORMAT:
    return new Snowflake::Client::ResultSetJson(json_rowset, metadata, tz_string);
  default:
    return nullptr;
  }
}

ResultSet* ResultSet::CreateResultFromChunk(void* initial_chunk,
                                           SF_COLUMN_DESC* metadata,
                                           QueryResultFormat query_result_format,
                                           const std::string& tz_string)
{
  switch (query_result_format)
  {
#ifndef SF_WIN32
  case SF_ARROW_FORMAT:
    return new Snowflake::Client::ResultSetArrow((arrow::BufferBuilder*)(((NON_JSON_RESP*)initial_chunk)->buffer), metadata, tz_string);
#endif
  case SF_JSON_FORMAT:
    return new Snowflake::Client::ResultSetJson((cJSON*)initial_chunk, metadata, tz_string);
  default:
    return nullptr;
  }
}

} // namespace Client
} // namespace Snowflake
