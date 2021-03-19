/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <time.h>

#include "../logger/SFLogger.hpp"
#include "ResultSetArrow.hpp"
#include "result_set_arrow.h"
#include "DataConversion.hpp"
#include "results.h"

#ifndef SF_WIN32
namespace Snowflake
{
namespace Client
{


ResultSetArrow::ResultSetArrow() :
    Snowflake::Client::ResultSet()
{
    m_queryResultFormat = QueryResultFormat::ARROW;
    m_isFirstChunk = true;
}

ResultSetArrow::ResultSetArrow(
    arrow::BufferBuilder* initialChunk,
    SF_COLUMN_DESC * metadata,
    std::string tzString
) :
    ResultSet(metadata, tzString)
{
    m_queryResultFormat = QueryResultFormat::ARROW;
    m_isFirstChunk = true;

    this->appendChunk(initialChunk);

    // Reset row indices so that they can be re-used by public API.
    m_currChunkIdx = 0;
    m_currChunkRowIdx = 0;
    m_currRowIdx = 0;
}

ResultSetArrow::~ResultSetArrow()
{
    ; // Do nothing.
}

// Public methods ==================================================================================

SF_STATUS STDCALL ResultSetArrow::appendChunk(arrow::BufferBuilder * chunk)
{
    CXX_LOG_INFO("appendChunk -- Chunk %d received.", m_currChunkIdx);
    m_currChunkIdx++;

    m_chunkIterator = std::make_shared<ArrowChunkIterator>(chunk, m_metadata, m_tzString);
    if (m_isFirstChunk)
    {
        m_isFirstChunk = false;
        m_totalColumnCount = m_chunkIterator->getColumnCount();
        m_cacheStrVal.resize(m_totalColumnCount);
        for (int i = 0; i < m_totalColumnCount; i++)
        {
            m_cacheStrVal[i].first = false;
        }
    }
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::finishResultSet()
{
    m_currChunkIdx = 0;
    m_currChunkRowIdx = 0;
    m_currColumnIdx = 0;
    m_currRowIdx = 0;

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::next()
{
    // clear cache for each row
    for (int i = 0; i < m_totalColumnCount; i++)
    {
      m_cacheStrVal[i].first = false;
    }

    if (!m_chunkIterator || !m_chunkIterator->next())
    {
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::getCellAsBool(size_t idx, sf_bool * out_data)
{
    return m_chunkIterator->getCellAsBool(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsInt8(size_t idx, int8 * out_data)
{
    return m_chunkIterator->getCellAsInt8(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsInt32(size_t idx, int32 * out_data)
{
    return m_chunkIterator->getCellAsInt32(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsInt64(size_t idx, int64 * out_data)
{
    return m_chunkIterator->getCellAsInt64(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsUint8(size_t idx, uint8 * out_data)
{
    return m_chunkIterator->getCellAsUint8(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsUint32(size_t idx, uint32 * out_data)
{
    return m_chunkIterator->getCellAsUint32(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsUint64(size_t idx, uint64 * out_data)
{
    return m_chunkIterator->getCellAsUint64(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsFloat32(size_t idx, float32 * out_data)
{
    return m_chunkIterator->getCellAsFloat32(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsFloat64(size_t idx, float64 * out_data)
{
    return m_chunkIterator->getCellAsFloat64(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsConstString(size_t idx, const char ** out_data)
{
    if ((0 == idx) || (idx > m_cacheStrVal.size()))
    {
      return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }
  
    sf_bool isNull = SF_BOOLEAN_FALSE;
    if (m_chunkIterator->isCellNull(idx - 1))
    {
        *out_data = NULL;
        return SF_STATUS_SUCCESS;
    }

    if (!m_cacheStrVal[idx - 1].first)
    {
        SF_STATUS ret = m_chunkIterator->getCellAsString(idx - 1, m_cacheStrVal[idx - 1].second);
        if (SF_STATUS_SUCCESS != ret)
        {
            return ret;
        }
        m_cacheStrVal[idx - 1].first = true;
    }
    
    *out_data = m_cacheStrVal[idx - 1].second.c_str();

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ResultSetArrow::getCellAsString(size_t idx, char ** out_data, size_t * io_len, size_t * io_capacity)
{
    if ((0 == idx) || (idx > m_cacheStrVal.size()))
    {
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (!m_cacheStrVal[idx - 1].first)
    {
        if (m_chunkIterator->isCellNull(idx - 1))
        {
            m_cacheStrVal[idx - 1].second = "";
        }
        else
        {
            SF_STATUS ret = m_chunkIterator->getCellAsString(idx - 1, m_cacheStrVal[idx - 1].second);
            if (SF_STATUS_SUCCESS != ret)
            {
                return ret;
            }
        }
        m_cacheStrVal[idx - 1].first = true;
    }
  
    std::string& strVal = m_cacheStrVal[idx - 1].second;
    Client::Util::AllocateCharBuffer(out_data, io_len, io_capacity, strVal.length());
    sb_strcpy(*out_data, strVal.length() + 1, strVal.c_str());
  
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data)
{
    return m_chunkIterator->getCellAsTimestamp(idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellStrlen(size_t idx, size_t * out_data)
{
    if ((0 == idx) || (idx > m_cacheStrVal.size()))
    {
      return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }
  
    if (m_chunkIterator->isCellNull(idx - 1))
    {
        out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    if (!m_cacheStrVal[idx - 1].first)
    {
        SF_STATUS ret = m_chunkIterator->getCellAsString(idx - 1, m_cacheStrVal[idx - 1].second);
        if (SF_STATUS_SUCCESS != ret)
        {
            return ret;
        }
        m_cacheStrVal[idx - 1].first = true;
    }

    *out_data = m_cacheStrVal[idx - 1].second.length();
    return SF_STATUS_SUCCESS;
}

size_t ResultSetArrow::getRowCountInChunk()
{
    CXX_LOG_TRACE("Retrieving row count in current chunk.");
    return m_chunkIterator->getRowCountInChunk();
}

SF_STATUS STDCALL ResultSetArrow::isCellNull(size_t idx, sf_bool * out_data)
{
    if ((0 == idx) || (idx > m_cacheStrVal.size()))
    {
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    *out_data = m_chunkIterator->isCellNull(idx - 1) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
    return SF_STATUS_SUCCESS;
}

// Private methods =================================================================================

} // namespace Client
} // namespace Snowflake

#endif // SF_WIN32
