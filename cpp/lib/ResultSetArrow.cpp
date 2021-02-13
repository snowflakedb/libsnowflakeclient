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
    m_bufferBuilder = std::make_shared<arrow::BufferBuilder>();
}

ResultSetArrow::ResultSetArrow(
    cJSON * initialChunk,
    SF_COLUMN_DESC * metadata,
    std::string tzString
) :
    ResultSet(metadata, tzString)
{
    m_queryResultFormat = QueryResultFormat::ARROW;
    m_bufferBuilder = std::make_shared<arrow::BufferBuilder>();
    init(initialChunk);
}

// Public methods ==================================================================================

SF_STATUS STDCALL ResultSetArrow::appendChunk(cJSON * chunk)
{
    // Decode Base64-encoded Arrow-format rowset of the chunk.
    const char * base64Rowset = snowflake_cJSON_GetStringValue(chunk);
    if (base64Rowset == NULL)
    {
        CXX_LOG_ERROR("Unable to retrieve value of key 'base64Rowset'.");
        return SF_STATUS_ERROR_BAD_RESPONSE;
    }

    std::string decodedRowset = arrow::util::base64_decode(std::string(base64Rowset));
    m_bufferBuilder->Append((void *)decodedRowset.c_str(), decodedRowset.length());

    CXX_LOG_TRACE("Chunk %d received.", m_currChunkIdx);
    m_currChunkIdx++;

    // Finalize the currently built buffer and initialize reader objects from it.
    // This resets the buffer builder for future use.
    std::shared_ptr<arrow::Buffer> arrowResultData;
    m_bufferBuilder->Finish(&arrowResultData);
    m_bufferReader = std::make_shared<arrow::io::BufferReader>(arrowResultData);
    arrow::ipc::RecordBatchStreamReader::Open(m_bufferReader, &m_batchReader);

    // Add the batches to a vector of RecordBatch objects.
    // This stores batches as they are retrieved from the server row-wise
    // and passes them to the ArrowChunkIterator.
    std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
    while (true)
    {
        std::shared_ptr<arrow::RecordBatch> batch;
        m_batchReader->ReadNext(&batch);
        if (batch == nullptr)
            break;
        batches.emplace_back(batch);
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

SF_STATUS STDCALL ResultSetArrow::nextColumn()
{
    if (m_currColumnIdx >= m_totalColumnCount)
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;

    m_currColumnIdx++;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::nextRow()
{
    if (m_currRowIdx >= m_totalRowCount)
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;

    m_currRowIdx++;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsBool(sf_bool * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsBool(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsInt8(int8 * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsInt8(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsInt32(int32 * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsInt32(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsInt64(int64 * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsInt64(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsUint8(uint8 * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsUint8(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsUint32(uint32 * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsUint32(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsUint64(uint64 * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsUint64(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsFloat64(float64 * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsFloat64(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsConstString(const char ** out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsConstString(m_currColumnIdx, m_currRowIdx, out_data);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsString(char * out_data, size_t * io_len, size_t * io_capacity)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsString(
        m_currColumnIdx, m_currRowIdx, out_data, io_len, io_capacity);
}

SF_STATUS STDCALL ResultSetArrow::getCurrCellAsTimestamp(SF_TIMESTAMP * out_data)
{
    std::shared_ptr<arrow::RecordBatch> rowData;
    return m_chunkIterator->getCellAsTimestamp(m_currColumnIdx, m_currRowIdx, out_data);
}

size_t ResultSetArrow::getRowCountInChunk()
{
    return m_chunkIterator->getRowCountInChunk();
}

// Private methods =================================================================================

void ResultSetArrow::init(cJSON * initialChunk)
{
    // Populate result set with chunks retrieved from server until all rows in
    // the chunk downloader are consumed.
    // Server responds with rows, but it's more convenient to store as columns.
    this->appendChunk(initialChunk);

    // Reset row indices so that they can be re-used by public API.
    m_currChunkIdx = 0;
    m_currChunkRowIdx = 0;
    m_currRowIdx = 0;
}

} // namespace Client
} // namespace Snowflake
