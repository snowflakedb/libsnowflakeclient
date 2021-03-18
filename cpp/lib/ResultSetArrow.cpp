/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <time.h>

#include "../logger/SFLogger.hpp"
#include "DataConversion.hpp"
#include "ResultSetArrow.hpp"
#include "result_set_arrow.h"
#include "results.h"


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
    cJSON * data,
    cJSON * rowset,
    SF_COLUMN_DESC * metadata,
    std::string tzString
) :
    ResultSet(data, rowset, metadata, tzString)
{
    m_queryResultFormat = QueryResultFormat::ARROW;
    m_isFirstChunk = true;

    // Populate result set with chunks retrieved from server until all rows in
    // the chunk downloader are consumed.
    // Server responds with rows, but it's more convenient to store as columns.
    this->appendChunk(rowset);

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

SF_STATUS STDCALL ResultSetArrow::appendChunk(cJSON * chunk)
{
    const char * base64RowsetStr = snowflake_cJSON_GetStringValue(chunk);

    if (base64RowsetStr == nullptr)
    {
        CXX_LOG_ERROR("appendChunk -- NULL value for key '%s' in current chunk.", m_rowsetKey);
        return SF_STATUS_ERROR_BAD_RESPONSE;
    }

    if (base64RowsetStr == "")
    {
        // Sometimes, the server will respond with an empty value for rowsetBase64
        // but will also supply a URL to download the chunk from.
        return SF_STATUS_SUCCESS;
    }

    // Decode Base64-encoded Arrow-format rowset of the chunk and build a buffer builder from it.
    std::string decodedRowsetStr = arrow::util::base64_decode(std::string(base64RowsetStr));
    std::shared_ptr<arrow::BufferBuilder> bufferBuilder = std::make_shared<arrow::BufferBuilder>();
    bufferBuilder->Append((void *) decodedRowsetStr.c_str(), decodedRowsetStr.length());

    CXX_LOG_INFO("appendChunk -- Chunk %d received.", m_currChunkIdx);
    m_currChunkIdx++;

    // Finalize the currently built buffer and initialize reader objects from it.
    // This resets the buffer builder for future use.
    std::shared_ptr<arrow::Buffer> arrowResultData;
    bufferBuilder->Finish(&arrowResultData);
    std::shared_ptr<arrow::io::BufferReader> bufferReader =
        std::make_shared<arrow::io::BufferReader>(arrowResultData);

    // Add the batches to a vector.
    // This stores batches as they are retrieved from the server row-wise
    // and passes them to the ArrowChunkIterator.
    arrow::Result<std::shared_ptr<arrow::ipc::RecordBatchReader>> batchReader =
        arrow::ipc::RecordBatchStreamReader::Open(std::move(bufferReader));

    std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
    while (true)
    {
        std::shared_ptr<arrow::RecordBatch> batch;
        batchReader.ValueOrDie()->ReadNext(&batch);
        if (batch == nullptr)
            break;
        batches.emplace_back(batch);
    }

    if (m_isFirstChunk)
    {
        m_chunkIterator = std::make_shared<ArrowChunkIterator>(m_metadata, m_tzString);
    }

    m_chunkIterator->updateCounts(&batches, &m_totalColumnCount, &m_totalRowCount);
    if (m_isFirstChunk)
    {
        m_isFirstChunk = false;
        m_cacheStrVal.resize(m_totalColumnCount);
        for (int i = 0; i < m_totalColumnCount; i++)
        {
            m_cacheStrVal[i].first = false;
        }
    }
    return m_chunkIterator->appendChunk(&batches);
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
    if (m_currColumnIdx >= m_totalColumnCount - 1)
    {
        CXX_LOG_ERROR("next -- Already reached end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    m_currColumnIdx++;
    // clear cache for each row
    for (int i = 0; i < m_totalColumnCount; i++)
    {
      m_cacheStrVal[i].first = false;
    }
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::getCellAsBool(size_t idx, sf_bool * out_data)
{
    return m_chunkIterator->getCellAsBool(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsInt8(size_t idx, int8 * out_data)
{
    return m_chunkIterator->getCellAsInt8(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsInt32(size_t idx, int32 * out_data)
{
    return m_chunkIterator->getCellAsInt32(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsInt64(size_t idx, int64 * out_data)
{
    return m_chunkIterator->getCellAsInt64(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsUint8(size_t idx, uint8 * out_data)
{
    return m_chunkIterator->getCellAsUint8(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsUint32(size_t idx, uint32 * out_data)
{
    return m_chunkIterator->getCellAsUint32(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsUint64(size_t idx, uint64 * out_data)
{
    return m_chunkIterator->getCellAsUint64(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsFloat32(size_t idx, float32 * out_data)
{
    return m_chunkIterator->getCellAsFloat32(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsFloat64(size_t idx, float64 * out_data)
{
    return m_chunkIterator->getCellAsFloat64(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellAsConstString(size_t idx, const char ** out_data)
{
    if ((0 == idx) || (idx >= m_cacheStrVal.size()))
    {
      return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }
  
    sf_bool isNull = SF_BOOLEAN_FALSE;
    SF_STATUS ret = m_chunkIterator->isCellNull(idx - 1, m_currRowIdx, &isNull);
    if (SF_STATUS_SUCCESS != ret)
    {
        return ret;
    }
    if (SF_BOOLEAN_TRUE == isNull)
    {
        *out_data = NULL;
        return SF_STATUS_SUCCESS;
    }

    if (!m_cacheStrVal[idx - 1].first)
    {
        SF_STATUS ret = m_chunkIterator->getCellAsString(idx - 1, m_currRowIdx, m_cacheStrVal[idx - 1].second);
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
  if ((0 == idx) || (idx >= m_cacheStrVal.size()))
  {
    return SF_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  sf_bool isNull = SF_BOOLEAN_FALSE;
  SF_STATUS ret = m_chunkIterator->isCellNull(idx - 1, m_currRowIdx, &isNull);
  if (SF_STATUS_SUCCESS != ret)
  {
    return ret;
  }
  if (SF_BOOLEAN_TRUE == isNull)
  {
    *out_data = 0;
    return SF_STATUS_SUCCESS;
  }

  if (!m_cacheStrVal[idx - 1].first)
  {
    SF_STATUS ret = m_chunkIterator->getCellAsString(idx - 1, m_currRowIdx, m_cacheStrVal[idx - 1].second);
    if (SF_STATUS_SUCCESS != ret)
    {
      return ret;
    }
    m_cacheStrVal[idx - 1].first = true;
  }

  // Allocate buffer as needed and copy the data.
  size_t len = m_cacheStrVal[idx - 1].second.size();
  Snowflake::Client::Util::AllocateCharBuffer(out_data, io_len, io_capacity, len);
  std::strncpy(*out_data, m_cacheStrVal[idx - 1].second.c_str(), len);
  (*out_data)[len] = '\0';

  return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data)
{
    return m_chunkIterator->getCellAsTimestamp(m_currColumnIdx, idx - 1, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCellStrlen(size_t idx, size_t * out_data)
{
    if ((0 == idx) || (idx >= m_cacheStrVal.size()))
    {
      return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }
  
    sf_bool isNull = SF_BOOLEAN_FALSE;
    SF_STATUS ret = m_chunkIterator->isCellNull(idx - 1, m_currRowIdx, &isNull);
    if (SF_STATUS_SUCCESS != ret)
    {
        return ret;
    }
    if (SF_BOOLEAN_TRUE == isNull)
    {
        out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    if (!m_cacheStrVal[idx - 1].first)
    {
        SF_STATUS ret = m_chunkIterator->getCellAsString(idx - 1, m_currRowIdx, m_cacheStrVal[idx - 1].second);
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
    return m_chunkIterator->isCellNull(m_currColumnIdx, idx - 1, out_data);
}

// Private methods =================================================================================

} // namespace Client
} // namespace Snowflake
