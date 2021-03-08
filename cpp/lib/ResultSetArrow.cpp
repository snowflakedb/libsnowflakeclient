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
    cJSON * initialChunk,
    SF_COLUMN_DESC * metadata,
    std::string tzString
) :
    ResultSet(metadata, tzString)
{
    m_queryResultFormat = QueryResultFormat::ARROW;
    m_isFirstChunk = true;

    // Populate result set with chunks retrieved from server until all rows in
    // the chunk downloader are consumed.
    // Server responds with rows, but it's more convenient to store as columns.
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

SF_STATUS STDCALL ResultSetArrow::appendChunk(cJSON * chunk)
{
    const char * base64RowsetStr = snowflake_cJSON_GetStringValue(chunk);
    CXX_LOG_DEBUG("base64RowsetStr: %s", base64RowsetStr);

    if (base64RowsetStr == nullptr)
    {
        CXX_LOG_ERROR("NULL value for key '%s' found in current chunk.", m_rowsetKey);
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

    CXX_LOG_DEBUG("Chunk %d received.", m_currChunkIdx);
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
        if (batch == nullptr) break;
        for (int i = 0; i < batch->schema()->num_fields(); ++i)
        {
            CXX_LOG_DEBUG(
                "appendChunk -- column %d is of type %d",
                i, batch->schema()->field(i)->type()->id());
        }
        batches.emplace_back(batch);
    }

    if (m_isFirstChunk)
    {
        m_isFirstChunk = false;
        m_chunkIterator = std::make_shared<ArrowChunkIterator>(m_metadata, m_tzString);
    }

    m_chunkIterator->updateCounts(&batches, &m_totalColumnCount, &m_totalRowCount);
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
    if (m_currRowIdx >= m_totalRowCount - 1)
    {
        if (m_currColumnIdx >= m_totalColumnCount - 1)
            return SF_STATUS_ERROR_OUT_OF_BOUNDS;

        m_currRowIdx = 0;
        m_currColumnIdx++;
        return SF_STATUS_SUCCESS;
    }

    m_currRowIdx++;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsBool(sf_bool * out_data)
{
    return m_chunkIterator->getCellAsBool(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsInt8(int8 * out_data)
{
    return m_chunkIterator->getCellAsInt8(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsInt32(int32 * out_data)
{
    return m_chunkIterator->getCellAsInt32(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsInt64(int64 * out_data)
{
    return m_chunkIterator->getCellAsInt64(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsUint8(uint8 * out_data)
{
    return m_chunkIterator->getCellAsUint8(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsUint32(uint32 * out_data)
{
    return m_chunkIterator->getCellAsUint32(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsUint64(uint64 * out_data)
{
    return m_chunkIterator->getCellAsUint64(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsFloat32(float32 * out_data)
{
    return m_chunkIterator->getCellAsFloat32(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsFloat64(float64 * out_data)
{
    return m_chunkIterator->getCellAsFloat64(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsConstString(const char ** out_data)
{
    return m_chunkIterator->getCellAsConstString(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL
ResultSetArrow::getCurrCellAsString(char ** out_data, size_t * io_len, size_t * io_capacity)
{
    return m_chunkIterator->getCellAsString(
        m_currColumnIdx, m_currRowIdx, out_data, io_len, io_capacity);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsTimestamp(SF_TIMESTAMP * out_data)
{
    return m_chunkIterator->getCellAsTimestamp(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellStrlen(size_t * out_data)
{
    return m_chunkIterator->getCellStrlen(m_currColumnIdx, m_currRowIdx, out_data);
}

size_t ResultSetArrow::getRowCountInChunk()
{
    return m_chunkIterator->getRowCountInChunk();
}

SF_STATUS STDCALL ResultSetArrow::isCurrCellNull(sf_bool * out_data)
{
    return m_chunkIterator->isCellNull(m_currColumnIdx, m_currRowIdx, out_data);
}

// Private methods =================================================================================

} // namespace Client
} // namespace Snowflake
