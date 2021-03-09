/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All right reserved.
 */

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>

#include "../logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "ArrowChunkIterator.hpp"
#include "DataConversion.hpp"
#include "ResultSet.hpp"

#ifndef SF_WIN32
#undef BOOL

namespace Snowflake
{
namespace Client
{

ArrowChunkIterator::ArrowChunkIterator(SF_COLUMN_DESC * metadata, std::string tzString)
    : m_metadata(metadata), m_tzString(tzString)
{
    m_isFirstBatch = true;
    m_currBatchIndex = 0;
    m_currRowIndexInBatch = 0;

    // Schema and column counts are deferred to updateCounts(),
    // where the initial chunk is available.
    m_batchCount = 0;
    m_rowCount = 0;
    m_rowCountInBatch = 0;
    m_rowCountInChunk = 0;
}

// Public methods ==================================================================================

void ArrowChunkIterator::updateCounts(
    std::vector<std::shared_ptr<arrow::RecordBatch>> * chunk,
    size_t * out_colCount,
    size_t * out_rowCount
)
{
    // If first batch, then initialize schema and column count to proper values.
    if (m_isFirstBatch)
    {
        m_schema = (*chunk)[0]->schema();
        m_columnCount = m_schema->num_fields();
        *out_colCount = m_columnCount;
        CXX_LOG_DEBUG("updateCounts -- result set contains %d columns", m_columnCount);
    }

    // Reset the row count in the current chunk to 0.
    m_rowCountInChunk = 0;

    // Note: The calculation in the following for-loop depends on m_batchCount
    // holding the number of batches excluding the given chunk. Thus, it must
    // be executed before m_batchCount is updated for the given chunk.
    for (uint32 batchIdx = 0; batchIdx < chunk->size(); ++batchIdx)
    {
        // Update row-related counts.
        m_rowCountInBatch = (*chunk)[batchIdx]->num_rows();
        m_rowCount += m_rowCountInBatch;
        m_rowCountInChunk += m_rowCountInBatch;
        CXX_LOG_DEBUG("updateCounts -- %d rows in batch.", m_rowCountInBatch);

        // If this is the first batch, then set prevMaxRowIdx to -1
        // to account for zero-based indexing.
        // m_batchCount will be initialized to 0 which is fine as 0 + 0 = 0.
        int32 prevMaxRowIdx = m_isFirstBatch ?
            -1 : m_batchRowIndexMap[m_batchRowIndexMap.size() - 1];
        m_isFirstBatch = false;

        // Update batch-row index map.
        CXX_LOG_DEBUG(
            "updateCounts -- inserting <%d, %d> into m_batchRowIndexMap.",
            m_batchCount + batchIdx, static_cast<uint32>(prevMaxRowIdx + m_rowCountInBatch));
        m_batchRowIndexMap.insert(
            std::pair<uint32, uint32>(
                // Key is the current batch index.
                // Add previous batch count to current batch index.
                // m_batchCount stores size and batchIdx is zero-indexed, which cancels out.
                // There is no need to increment/decrement by one.
                m_batchCount + batchIdx,
                // Value is the current max row index.
                // Add previous max row index to current row count.
                static_cast<uint32>(prevMaxRowIdx + m_rowCountInBatch)));
    }

    m_batchCount += chunk->size();

    // Update ResulSetArrow counts.
    *out_rowCount += m_rowCountInChunk;
}

SF_STATUS STDCALL
ArrowChunkIterator::appendChunk(std::vector<std::shared_ptr<arrow::RecordBatch>> * chunk)
{
    // Iterate over all columns in the chunk, collecting them into the internal result set.
    for (uint32 i = 0; i < m_batchCount; ++i)
    {
        CXX_LOG_DEBUG("appendChunk -- Iterating on batch %d.", i);
        std::shared_ptr<arrow::RecordBatch> currBatch = (*chunk)[i];
        m_rowCountInBatch = currBatch->num_rows();

        for (uint32 j = 0; j < m_columnCount; ++j)
        {
            CXX_LOG_DEBUG("appendChunk -- Iterating on column %d.", j);
            std::shared_ptr<arrow::Array> colArray = currBatch->column(j);
            CXX_LOG_DEBUG("appendChunk -- RecordBatch column at 0x%x", colArray.get());
            std::shared_ptr<arrow::DataType> type = m_schema->field(j)->type();
            auto arrowCol = std::make_shared<ArrowColumn>(type);
            CXX_LOG_DEBUG(
                "appendChunk -- Created ArrowColumn for type %d at 0x%x",
                type->id(), arrowCol.get());

            switch (type->id())
            {
                case arrow::Type::type::BINARY:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is binary type.");
                    arrowCol->arrowBinary =
                        std::static_pointer_cast<arrow::BinaryArray>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::BOOL:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is boolean type.");
                    arrowCol->arrowBoolean =
                        std::static_pointer_cast<arrow::BooleanArray>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::DATE32:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is date32 type.");
                    arrowCol->arrowDate32 =
                        std::static_pointer_cast<arrow::Date32Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::DATE64:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is date64 type.");
                    arrowCol->arrowDate64 =
                        std::static_pointer_cast<arrow::Date64Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::DECIMAL:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is decimal128 type.");
                    arrowCol->arrowDecimal128 =
                        std::static_pointer_cast<arrow::Decimal128Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::DOUBLE:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is double type.");
                    arrowCol->arrowDouble =
                        std::static_pointer_cast<arrow::DoubleArray>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::INT8:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is int8 type.");
                    arrowCol->arrowInt8 =
                        std::static_pointer_cast<arrow::Int8Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::INT16:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is int16 type.");
                    arrowCol->arrowInt16 =
                        std::static_pointer_cast<arrow::Int16Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::INT32:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is int32 type.");
                    arrowCol->arrowInt32 =
                        std::static_pointer_cast<arrow::Int32Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::INT64:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is int64 type.");
                    arrowCol->arrowInt64 =
                        std::static_pointer_cast<arrow::Int64Array>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::type::STRING:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is string type.");
                    arrowCol->arrowString =
                        std::static_pointer_cast<arrow::StringArray>(colArray).get();
                    m_columns.emplace_back(arrowCol);
                    break;
                }
                case arrow::Type::STRUCT:
                {
                    CXX_LOG_DEBUG("appendChunk -- Column is struct type.");
                    auto values = std::static_pointer_cast<arrow::StructArray>(colArray);
                    ArrowTimestampArray * ts(new ArrowTimestampArray);

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
                    CXX_LOG_ERROR("Unsupported Arrow type for append: %d.", type->id());
                    return SF_STATUS_ERROR_DATA_CONVERSION;
                }
            }
        }
    }

    m_rowCountInBatch = 0;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsBool(uint32 colIdx, uint32 rowIdx, sf_bool * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Only support proper booleans.
    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (arrowType->id() != arrow::Type::type::BOOL)
    {
        CXX_LOG_ERROR("Unsupported conversion from %d to BOOLEAN.", arrowType->id());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (colData->arrowBoolean->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at column %d, row %d is null.", rowIdx, colIdx);
        return SF_STATUS_SUCCESS;
    }

    bool rawData = colData->arrowBoolean->Value(cellIdx);
    *out_data = (rawData) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsInt8(uint32 colIdx, uint32 rowIdx, int8 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_ERROR("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    int64 rawData;
    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch (arrowType->id())
    {
        case arrow::Type::type::BOOL:
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
        case arrow::Type::type::DECIMAL:
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
        {
            status = Conversion::Arrow::NumericToSignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, &rawData, INT8);
        }
        case arrow::Type::type::STRING:
        {
            status = Conversion::Arrow::StringToSignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, &rawData, INT8);
        }
        default:
        {
            CXX_LOG_ERROR("Unsupported conversion from %d to INT8.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    if (status != SF_STATUS_SUCCESS)
        return status;

    *out_data = static_cast<int8>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsInt32(uint32 colIdx, uint32 rowIdx, int32 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_ERROR("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG("Cell at column %d, row %d is null.", colIdx, rowIdx);
        return SF_STATUS_SUCCESS;
    }

    int64 rawData;
    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch (arrowType->id())
    {
        case arrow::Type::type::BOOL:
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
        case arrow::Type::type::DECIMAL:
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
        {
            status = Conversion::Arrow::NumericToSignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, &rawData, INT32);
        }
        case arrow::Type::type::STRING:
        {
            status = Conversion::Arrow::StringToSignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, &rawData, INT32);
        }
        default:
        {
            CXX_LOG_ERROR("Unsupported conversion from %d to INT32.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    if (status != SF_STATUS_SUCCESS)
        return status;

    *out_data = static_cast<int32>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsInt64(uint32 colIdx, uint32 rowIdx, int64 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG("Cell at column %d, row %d is null.", colIdx, rowIdx);
        return SF_STATUS_SUCCESS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch (arrowType->id())
    {
        case arrow::Type::type::BOOL:
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
        case arrow::Type::type::DECIMAL:
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
            return Conversion::Arrow::NumericToSignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, INT64);
        case arrow::Type::type::STRING:
            return Conversion::Arrow::StringToSignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, INT64);
        default:
            CXX_LOG_ERROR("Unsupported conversion from %d to INT64.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsUint8(uint32 colIdx, uint32 rowIdx, uint8 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG("Cell at column %d, row %d is null.", colIdx, rowIdx);
        return SF_STATUS_SUCCESS;
    }

    uint64 rawData;
    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch (arrowType->id())
    {
        case arrow::Type::type::BOOL:
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
        case arrow::Type::type::DECIMAL:
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
        {
            status = Conversion::Arrow::NumericToUnsignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, &rawData, UINT8);
        }
        case arrow::Type::type::STRING:
        {
            status = Conversion::Arrow::StringToUnsignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, &rawData, UINT8);
        }
        default:
        {
            CXX_LOG_ERROR("Unsupported conversion from %d to UINT8.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    if (status != SF_STATUS_SUCCESS)
        return status;

    *out_data = static_cast<uint8>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsUint32(uint32 colIdx, uint32 rowIdx, uint32 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG("Cell at column %d, row %d is null.", colIdx, rowIdx);
        return SF_STATUS_SUCCESS;
    }

    uint64 rawData;
    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch (arrowType->id())
    {
        case arrow::Type::type::BOOL:
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
        case arrow::Type::type::DECIMAL:
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
        {
            status = Conversion::Arrow::NumericToUnsignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, &rawData, UINT32);
        }
        case arrow::Type::type::STRING:
        {
            status = Conversion::Arrow::StringToUnsignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, &rawData, UINT32);
        }
        default:
        {
            CXX_LOG_ERROR("Unsupported conversion from %d to UINT32.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    if (status != SF_STATUS_SUCCESS)
        return status;

    *out_data = static_cast<uint32>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsUint64(uint32 colIdx, uint32 rowIdx, uint64 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG("Cell at column %d, row %d is null.", colIdx, rowIdx);
        return SF_STATUS_SUCCESS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch (arrowType->id())
    {
        case arrow::Type::type::BOOL:
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
        case arrow::Type::type::DECIMAL:
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
            return Conversion::Arrow::NumericToUnsignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, UINT64);
        case arrow::Type::type::STRING:
            return Conversion::Arrow::StringToUnsignedInteger(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, UINT64);
        default:
            CXX_LOG_ERROR("Unsupported conversion from %d to UINT64.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsFloat32(uint32 colIdx, uint32 rowIdx, float32 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG("Cell at column %d, row %d is null.", colIdx, rowIdx);
        return SF_STATUS_SUCCESS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch (arrowType->id())
    {
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
            return Conversion::Arrow::NumericToFloat(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::STRING:
            return Conversion::Arrow::StringToFloat(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        default:
            CXX_LOG_ERROR("Unsupported conversion from %d to FLOAT32.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsFloat64(uint32 colIdx, uint32 rowIdx, float64 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG("Cell at column %d, row %d is null.", colIdx, rowIdx);
        return SF_STATUS_SUCCESS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch (arrowType->id())
    {
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
            return Conversion::Arrow::NumericToDouble(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::STRING:
            return Conversion::Arrow::StringToDouble(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        default:
            CXX_LOG_ERROR("Unsupported conversion from %d to FLOAT64.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsConstString(uint32 colIdx, uint32 rowIdx, const char ** out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG(
            "Cell at column %d, row %d is null. Returning a null pointer as per spec.",
            colIdx, rowIdx);

        if (*out_data != nullptr)
        {
            delete *out_data;
            *out_data = nullptr;
        }

        return SF_STATUS_SUCCESS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    SF_DB_TYPE snowType = m_metadata[colIdx].type;
    int64 scale = m_metadata[colIdx].scale;

    switch (arrowType->id())
    {
        case arrow::Type::type::BINARY:
            return Conversion::Arrow::BinaryToConstString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::BOOL:
            return Conversion::Arrow::BoolToConstString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
            return Conversion::Arrow::DateToConstString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::DECIMAL:
            return Conversion::Arrow::DecimalToConstString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::DOUBLE:
            return Conversion::Arrow::DoubleToConstString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
            return Conversion::Arrow::IntToConstString(
                colData, arrowType, snowType, scale, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::STRUCT:
            return Conversion::Arrow::TimestampToConstString(
                colData, arrowType, snowType, scale, colIdx, rowIdx, cellIdx, out_data);
        case arrow::Type::type::STRING:
        {
            std::string strValue = colData->arrowString->GetString(cellIdx);
            *out_data = strValue.c_str();
            return SF_STATUS_SUCCESS;
        }
        default:
            CXX_LOG_ERROR("Unsupported conversion from %d to STRING.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsString(
    uint32 colIdx,
    uint32 rowIdx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    sf_bool isNull;
    SF_STATUS status = isCellNull(colIdx, rowIdx, &isNull);
    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Error while checking cell at column %d, row %d.", colIdx, rowIdx);
        return status;
    }

    if (isNull == SF_BOOLEAN_TRUE)
    {
        CXX_LOG_DEBUG(
            "Cell at column %d, row %d is null. Writing as empty string.",
            colIdx, rowIdx);

        // Trivial allocation if necessary.
        if (io_len == nullptr)
            io_len = new size_t;
        *io_len = 0;

        if (io_capacity == nullptr)
            io_capacity = new size_t;
        *io_capacity = 1;

        if (*out_data != nullptr)
            delete *out_data;
        *out_data = new char[*io_capacity];
        (*out_data)[0] = '\0';

        return SF_STATUS_SUCCESS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    SF_DB_TYPE snowType = m_metadata[colIdx].type;
    int64 scale = m_metadata[colIdx].scale;
    switch (arrowType->id())
    {
        case arrow::Type::type::BINARY:
            CXX_LOG_DEBUG("Converting from BINARY to STRING.");
            return Conversion::Arrow::BinaryToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, io_len, io_capacity);
        case arrow::Type::type::BOOL:
            CXX_LOG_DEBUG("Converting from BOOL to STRING.");
            return Conversion::Arrow::BoolToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, io_len, io_capacity);
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
            CXX_LOG_DEBUG("Converting from DATE to STRING.");
            return Conversion::Arrow::DateToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, io_len, io_capacity);
        case arrow::Type::type::DECIMAL:
            CXX_LOG_DEBUG("Converting from DECIMAL to STRING");
            return Conversion::Arrow::DecimalToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, io_len, io_capacity);
        case arrow::Type::type::DOUBLE:
            CXX_LOG_DEBUG("Converting from DOUBLE to STRING");
            return Conversion::Arrow::DoubleToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, out_data, io_len, io_capacity);
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
            CXX_LOG_DEBUG("Converting from INT to STRING.");
            return Conversion::Arrow::IntToString(
                colData, arrowType, snowType, scale,
                colIdx, rowIdx, cellIdx, out_data, io_len, io_capacity);
        case arrow::Type::type::STRUCT:
            CXX_LOG_DEBUG("Converting from STRUCT to STRING.");
            return Conversion::Arrow::TimestampToString(
                colData, arrowType, snowType, scale,
                colIdx, rowIdx, cellIdx, out_data, io_len, io_capacity);
        case arrow::Type::type::STRING:
        {
            CXX_LOG_DEBUG("Writing STRING data directly.");
            std::string strValue = colData->arrowString->GetString(cellIdx);
            Snowflake::Client::Util::AllocateCharBuffer(
                out_data,
                io_len,
                io_capacity,
                strValue.size());
            std::strncpy(*out_data, strValue.c_str(), *io_len);
            (*out_data)[*io_len] = '\0';

            return SF_STATUS_SUCCESS;
        }
        default:
            CXX_LOG_ERROR("Unsupported conversion from %d to STRING.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsTimestamp(uint32 colIdx, uint32 rowIdx, SF_TIMESTAMP * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Only support proper timestamps.
    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (arrowType->id() != arrow::Type::type::STRUCT)
    {
        CXX_LOG_ERROR("Unsupported conversion from %d to TIMESTAMP.", arrowType->id());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (colData->arrowTimestamp->sse->IsNull(cellIdx))
    {
        CXX_LOG_TRACE("Cell at column %d, row %d is null.", rowIdx, colIdx);
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
    out_data->ts_type = m_metadata[colIdx].type;

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellStrlen(uint32 colIdx, uint32 rowIdx, size_t * out_data)
{
    char * strValue = nullptr;
    size_t len = 0;
    size_t capacity = 0;

    SF_STATUS status = getCellAsString(colIdx, rowIdx, &strValue, &len, &capacity);
    if (status != SF_STATUS_SUCCESS)
        return status;

    *out_data = std::strlen(strValue);
    return SF_STATUS_SUCCESS;
}

std::shared_ptr<arrow::DataType> ArrowChunkIterator::getColumnArrowDataType(uint32 colIdx)
{
    return m_schema->field(colIdx)->type();
}

size_t ArrowChunkIterator::getRowCountInChunk()
{
    return m_rowCountInChunk;
}

SF_STATUS STDCALL
ArrowChunkIterator::isCellNull(uint32 colIdx, uint32 rowIdx, sf_bool * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);
    if (cellIdx < 0)
    {
        CXX_LOG_TRACE("Cell at column %d, row %d was not found.", colIdx, rowIdx);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = getColumnArrowDataType(colIdx);
    CXX_LOG_DEBUG("isCellNull -- type: %d", arrowType->id());

    switch (arrowType->id())
    {
        case arrow::Type::type::BINARY:
        {
            *out_data = colData->arrowBinary->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::BOOL:
        {
            *out_data = colData->arrowBoolean->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::DATE32:
        {
            *out_data = colData->arrowDate32->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::DATE64:
        {
            *out_data = colData->arrowDate64->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::DECIMAL:
        {
            *out_data = colData->arrowDecimal128->IsNull(cellIdx) ?
                SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::DOUBLE:
        {
            *out_data = colData->arrowDouble->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::INT8:
        {
            *out_data = colData->arrowInt8->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::INT16:
        {
            *out_data = colData->arrowInt16->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::INT32:
        {
            *out_data = colData->arrowInt32->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::INT64:
        {
            *out_data = colData->arrowInt64->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::STRING:
        {
            *out_data = colData->arrowString->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::STRUCT:
        {
            *out_data = colData->arrowTimestamp->sse->IsNull(cellIdx) ?
                SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        default:
        {
            CXX_LOG_ERROR("Unsupported Arrow type: %d.", arrowType->id());
            return SF_STATUS_ERROR_DATA_CONVERSION;
        }
    }

    return SF_STATUS_SUCCESS;
}

// Private methods =================================================================================

std::shared_ptr<ArrowColumn> ArrowChunkIterator::getColumn(
    uint32 colIdx,
    uint32 rowIdx,
    int32 * out_cellIdx
)
{
    CXX_LOG_DEBUG("getColumn -- col %d, row %d", colIdx, rowIdx);
    // The internal result set, m_columns, contains more elements than m_columnCount would suggest.
    // In fact, we have m_batchCount * m_columnCount elements altogether.
    // The columns are "chunked" in the sense that there is one set of columns for each RecordBatch.
    // Thus, we use the row index of the cell whose value we wish to retrieve to determine
    // the correct column data to write.
    uint32 prevMaxRowIdx = 0;

    for (auto const& batchRowIdxPair : m_batchRowIndexMap)
    {
        CXX_LOG_DEBUG(
            "getColumn -- curr pair is <%d, %d>",
            batchRowIdxPair.first, batchRowIdxPair.second);
        if (rowIdx <= batchRowIdxPair.second)
        {
            CXX_LOG_DEBUG("getColumn -- cellIdx %d", rowIdx - prevMaxRowIdx);
            *out_cellIdx = rowIdx - prevMaxRowIdx;

            // Since there is one set of columns stored for each batch, simply
            // multiply batch index with column count and add the given column index.
            uint32 trueColIdx = m_columnCount * batchRowIdxPair.first + colIdx;
            CXX_LOG_DEBUG("getColumn -- trueColIdx %d", trueColIdx);
            CXX_LOG_DEBUG(
                "getColumn -- m_columns[%d] at 0x%x",
                trueColIdx, m_columns[trueColIdx].get());
            return m_columns[trueColIdx];
        }
        else
        {
            CXX_LOG_DEBUG("getColumn -- prevMaxRowIdx %d", batchRowIdxPair.second);
            prevMaxRowIdx = batchRowIdxPair.second;
        }
    }

    // Should never reach.
    CXX_LOG_ERROR("Could not find the appropriate column element.");
    return nullptr;
}

} // namespace Client
} // namespace Snowflake
#endif // ifndef SF_WIN32
