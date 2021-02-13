/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All right reserved.
 */

#include <cstdint>
#include <ctime>
#include <stdlib.h>
#include <time.h>

#include "../logger/SFLogger.hpp"
#include "ArrowChunkIterator.hpp"
#ifndef SF_WIN32
#undef BOOL

namespace Snowflake
{
namespace Client
{

ArrowChunkIterator::ArrowChunkIterator(
    std::vector<std::shared_ptr<arrow::RecordBatch>> * initialChunk,
    SF_COLUMN_DESC * metadata
)
:
    m_metadata(metadata)
{
    m_isFirstBatch = true;
    m_currBatchIndex = 0;
    m_currRowIndexInBatch = 0;
    m_columnCount = 0;
    m_rowCount = 0;
    m_schema = (*initialChunk)[0]->schema();

    this->appendChunk(initialChunk);
}

// Public methods ==================================================================================

SF_STATUS STDCALL
ArrowChunkIterator::appendChunk(std::vector<std::shared_ptr<arrow::RecordBatch>> * chunk)
{
    // Update internal count and other statistic-holding members.
    this->updateCounts(chunk);

    // Iterate over all columns in the chunk, collecting them into the internal result set.
    for (uint32 i = 0; i < m_batchCount; ++i)
    {
        std::shared_ptr<arrow::RecordBatch> currBatch = (*chunk)[i];
        m_rowCountInBatch = currBatch->num_rows();

        for (uint32 j = 0; j < m_columnCount; ++j)
        {
            std::shared_ptr<arrow::Array> colArray = currBatch->column(j);
            std::shared_ptr<arrow::DataType> type = m_schema->field(j)->type();
            auto arrowCol = std::make_shared<ArrowColumn>(type);

            switch (type->id())
            {
                case arrow::Type::type::BINARY:
                {
                    arrowCol->arrowBinary =
                        std::static_pointer_cast<arrow::BinaryArray>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::BOOL:
                {
                    arrowCol->arrowBoolean =
                        std::static_pointer_cast<arrow::BooleanArray>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::DATE32:
                {
                    arrowCol->arrowDate32 =
                        std::static_pointer_cast<arrow::Date32Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::DATE64:
                {
                    arrowCol->arrowDate64 =
                        std::static_pointer_cast<arrow::Date64Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::DECIMAL:
                {
                    arrowCol->arrowDecimal128 =
                        std::static_pointer_cast<arrow::Decimal128Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::DOUBLE:
                {
                    arrowCol->arrowDouble =
                        std::static_pointer_cast<arrow::DoubleArray>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::INT8:
                {
                    arrowCol->arrowInt8 =
                        std::static_pointer_cast<arrow::Int8Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::INT16:
                {
                    arrowCol->arrowInt16 =
                        std::static_pointer_cast<arrow::Int16Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::INT32:
                {
                    arrowCol->arrowInt32 =
                        std::static_pointer_cast<arrow::Int32Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::INT64:
                {
                    arrowCol->arrowInt64 =
                        std::static_pointer_cast<arrow::Int64Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::STRING:
                {
                    arrowCol->arrowString =
                        std::static_pointer_cast<arrow::StringArray>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::STRUCT:
                {
                    auto values = std::static_pointer_cast<arrow::StructArray>(colArray);
                    ArrowTimestampArray * ts;

                    // Timestamp values are struct arrays which may contain the following fields:
                    // (1) Seconds since Epoch,
                    // (2) Fractional seconds, expressed in milliseconds,
                    // (3) Time zone offset.
                    // The only field guaranteed is (1). All others may or may not be present,
                    // depending on the scale and type of Timestamp (LTZ, NTZ, or TZ).
                    ts->sse = std::static_pointer_cast<arrow::Int64Array>(values->field(0));
                    if (values->num_fields() > 1)
                        ts->fs = std::static_pointer_cast<arrow::Int32Array>(values->field(1));
                    if (values->num_fields() > 2)
                        ts->tz = std::static_pointer_cast<arrow::Int32Array>(values->field(2));

                    arrowCol->arrowTimestamp = ts;
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                default:
                {
                    CXX_LOG_ERROR("Unsupported Arrow type for append: %s.", type->name());
                    return SF_STATUS_ERROR_DATA_CONVERSION;
                }
            }
        }
    }

    m_rowCountInBatch = 0;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsBool(uint32 colIdx, uint32 rowIdx, sf_bool * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowBoolean->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    bool rawData = colData->arrowBoolean->Value(cellIdx);
    *out_data = (rawData) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsInt8(uint32 colIdx, uint32 rowIdx, int8 * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowInt8->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    int8_t rawData = colData->arrowInt8->Value(cellIdx);
    *out_data = static_cast<int8>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsInt32(uint32 colIdx, uint32 rowIdx, int32 * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowInt32->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    int32_t rawData = colData->arrowInt32->Value(cellIdx);
    *out_data = static_cast<int32>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsInt64(uint32 colIdx, uint32 rowIdx, int64 * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowInt64->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    int64_t rawData = colData->arrowInt64->Value(cellIdx);
    *out_data = static_cast<int64>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsUint8(uint32 colIdx, uint32 rowIdx, uint8 * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowInt8->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    int8_t rawData = colData->arrowInt8->Value(cellIdx);
    *out_data = static_cast<uint8>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsUint32(uint32 colIdx, uint32 rowIdx, uint32 * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowInt32->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    int32_t rawData = colData->arrowInt32->Value(cellIdx);
    *out_data = static_cast<uint32>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsUint64(uint32 colIdx, uint32 rowIdx, uint64 * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowInt64->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    int64_t rawData = colData->arrowInt64->Value(cellIdx);
    *out_data = static_cast<int64>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsFloat64(uint32 colIdx, uint32 rowIdx, float64 * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowDouble->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    // Double values are already stored as float64 so there is no need to cast->
    *out_data = colData->arrowDouble->Value(cellIdx);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsConstString(uint32 colIdx, uint32 rowIdx, const char ** out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowString->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    std::string rawData = colData->arrowString->GetString(cellIdx);
    *out_data = rawData.c_str();
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsString(
    uint32 colIdx,
    uint32 rowIdx,
    char * out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowString->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    // TODO: Support the following data types:
    // - Boolean
    // - Date
    // - Time
    // - Timestamp
    std::string rawData = colData->arrowString->GetString(cellIdx);
    *io_len = rawData.size();

    // Check if there is a pre-allocated buffer passed.
    bool isPreallocated = out_data != nullptr
        && io_capacity != nullptr
        && io_capacity > 0;

    // Allocate a new buffer if there is no pre-allocated buffer,
    // or if the provided buffer is not of adequate size.
    if (!isPreallocated)
    {
        io_capacity = new size_t;
        *io_capacity = rawData.size();
        out_data = new char[*io_capacity];
    }

    if (isPreallocated && *io_len > *io_capacity)
    {
        *io_capacity = rawData.size();
        delete out_data;
        out_data = new char[*io_capacity];
    }

    std::strcpy(out_data, rawData.c_str());
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsTimestamp(uint32 colIdx, uint32 rowIdx, SF_TIMESTAMP * out_data)
{
    std::shared_ptr<ArrowColumn> colData(nullptr);
    int32 cellIdx = this->getColumn(colIdx, rowIdx, colData);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (colData->arrowTimestamp->sse->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at row %d, column %d is null.", rowIdx, colIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    // The C API has a custom struct `SF_TIMESTAMP` used to represent Timestamp values.
    // In addition to what's available in the ArrowTimestamp struct, we must provide:
    // (1) A tm object that represents the timestamp value,
    // (2) The scale of the value,
    // (3) The timestamp type, i.e. LTZ, NTZ, TZ.
    ArrowTimestampArray * ts = colData->arrowTimestamp;
    time_t t = static_cast<time_t>(ts->sse->Value(cellIdx));
    out_data->tm_obj = *(std::localtime(&t));
    out_data->nsec = ts->fs->Value(cellIdx);
    out_data->tzoffset = ts->tz->Value(cellIdx);
    out_data->scale = static_cast<int64>(m_metadata[colIdx].scale);
    out_data->ts_type = this->getColumnSnowflakeDataType(colIdx);

    return SF_STATUS_SUCCESS;
}

std::shared_ptr<arrow::DataType> ArrowChunkIterator::getColumnArrowDataType(uint32 colIdx)
{
    return m_schema->field(colIdx)->type();
}

SF_DB_TYPE ArrowChunkIterator::getColumnSnowflakeDataType(uint32 colIdx)
{
    return m_metadata[colIdx].type;
}

size_t ArrowChunkIterator::getRowCountInChunk()
{
    return m_rowCountInChunk;
}

// Private methods =================================================================================

void ArrowChunkIterator::updateCounts(std::vector<std::shared_ptr<arrow::RecordBatch>> * chunk)
{
    // Reset the row count in the current chunk to 0.
    m_rowCountInChunk = 0;

    // Note: The calculation in the following for-loop depends on m_batchCount
    // holding the number of batches excluding the given chunk. Thus, it must
    // be executed before m_batchCount is updated for the given chunk.
    for (uint32 batchIdx = 0; batchIdx < chunk->size(); ++batchIdx)
    {
        // Update row count. Piggyback and update chunk row count while we're looping anyway.
        m_rowCount += (*chunk)[batchIdx]->num_rows();
        m_rowCountInChunk += (*chunk)[batchIdx]->num_rows();

        // If this is the first batch, then subtract calculated values by 1
        // to account for zero-based indexing.
        uint32 c = m_isFirstBatch ? 1 : 0;
        m_isFirstBatch = false;

        uint32 prevMaxRowIdx = m_batchRowIndexMap[m_batchRowIndexMap.size() - 1];

        // Update batch-row index map.
        m_batchRowIndexMap.insert(
            std::pair<uint32, uint32>(
                // Key is the current batch index.
                // Add previous batch count to current batch index.
                // m_batchCount stores size and batchIdx is zero-indexed, which cancels out.
                // There is no need to increment/decrement by one.
                m_batchCount + batchIdx,
                // Value is the current max row index.
                // Add previous max row index to current row count.
                prevMaxRowIdx + (*chunk)[batchIdx]->num_rows() - c));
    }

    m_batchCount += chunk->size();
    m_rowCountInBatch = m_batchCount > 0 ? (*chunk)[0]->num_rows() : 0;
}

int32 ArrowChunkIterator::getColumn(
    uint32 colIdx,
    uint32 rowIdx,
    std::shared_ptr<ArrowColumn> out_colData
)
{
    // The internal result set, m_columns, contains more elements than m_columnCount would suggest.
    // In fact, we have m_batchCount * m_columnCount elements altogether.
    // The columns are "chunked" in the sense that there is one set of columns for each RecordBatch.
    // Thus, we use the row index of the cell whose value we wish to retrieve to determine
    // the correct column data to write.
    uint32 prevMaxRowIdx = 0;

    for (auto const& batchRowIdxPair : m_batchRowIndexMap)
    {
        if (rowIdx <= batchRowIdxPair.second)
        {
            // Since there is one set of columns stored for each batch, simply
            // multiply batch index with column count and add the given column index.
            uint32 trueColIdx = m_columnCount * batchRowIdxPair.first + colIdx;
            out_colData = m_columns[trueColIdx];

            return rowIdx - prevMaxRowIdx;
        }
        else
        {
            prevMaxRowIdx = batchRowIdxPair.second;
        }
    }

    // Should never reach.
    CXX_LOG_ERROR("Could not find the appropriate column element.");
    return -1;
}

} // namespace Client
} // namespace Snowflake
#endif // ifndef SF_WIN32
