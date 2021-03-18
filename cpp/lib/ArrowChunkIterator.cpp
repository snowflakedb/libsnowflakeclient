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
        CXX_LOG_TRACE("updateCounts -- result set contains %d columns", m_columnCount);
    }

    // Reset the row count in the current chunk to 0.
    m_rowCountInChunk = 0;

    // Note: The calculation in the following for-loop depends on m_batchCount
    // holding the number of batches excluding the given chunk. Thus, it must
    // be executed before m_batchCount is updated for the given chunk.
    for (size_t batchIdx = 0; batchIdx < chunk->size(); ++batchIdx)
    {
        // Update row-related counts.
        m_rowCountInBatch = (*chunk)[batchIdx]->num_rows();
        m_rowCount += m_rowCountInBatch;
        m_rowCountInChunk += m_rowCountInBatch;

        // If this is the first batch, then set prevMaxRowIdx to -1
        // to account for zero-based indexing.
        // m_batchCount will be initialized to 0 which is fine as 0 + 0 = 0.
        int32 prevMaxRowIdx = m_isFirstBatch ?
            -1 : m_batchRowIndexMap[m_batchRowIndexMap.size() - 1];
        m_isFirstBatch = false;

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
                static_cast<uint32>(prevMaxRowIdx + m_rowCountInBatch)));
    }

    m_batchCount += chunk->size();

    // Update ResulSetArrow counts.
    *out_rowCount += m_rowCountInChunk;
    CXX_LOG_TRACE("result set contains %d rows", *out_rowCount);
}

SF_STATUS STDCALL
ArrowChunkIterator::appendChunk(std::vector<std::shared_ptr<arrow::RecordBatch>> * chunk)
{
    // Iterate over all columns in the chunk, collecting them into the internal result set.
    for (size_t i = 0; i < m_batchCount; ++i)
    {
        std::shared_ptr<arrow::RecordBatch> currBatch = (*chunk)[i];
        m_rowCountInBatch = currBatch->num_rows();

        for (size_t j = 0; j < m_columnCount; ++j)
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
ArrowChunkIterator::getCellAsBool(size_t colIdx, size_t rowIdx, sf_bool * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = SF_BOOLEAN_FALSE;
        return SF_STATUS_SUCCESS;
    }

    switch (arrowType->id())
    {
        case arrow::Type::type::BOOL:
        {
            auto rawData = colData->arrowBoolean->Value(cellIdx);
            *out_data = (rawData) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::DOUBLE:
        {
            auto rawData = colData->arrowDouble->Value(cellIdx);
            *out_data = (rawData == 0.0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::INT8:
        {
            auto rawData = colData->arrowInt8->Value(cellIdx);
            *out_data = (rawData == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::INT16:
        {
            auto rawData = colData->arrowInt16->Value(cellIdx);
            *out_data = (rawData == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::INT32:
        {
            auto rawData = colData->arrowInt32->Value(cellIdx);
            *out_data = (rawData == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::INT64:
        {
            auto rawData = colData->arrowInt64->Value(cellIdx);
            *out_data = (rawData == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::STRING:
        {
            auto rawData = colData->arrowString->GetString(cellIdx);
            *out_data = (rawData.empty()) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        default:
        {
            CXX_LOG_ERROR("Unsupported conversion from %d to BOOLEAN.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsInt8(size_t colIdx, size_t rowIdx, int8 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    int64 rawData;
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
ArrowChunkIterator::getCellAsInt32(size_t colIdx, size_t rowIdx, int32 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    int64 rawData;
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
ArrowChunkIterator::getCellAsInt64(size_t colIdx, size_t rowIdx, int64 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

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
ArrowChunkIterator::getCellAsUint8(size_t colIdx, size_t rowIdx, uint8 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    uint64 rawData;
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
ArrowChunkIterator::getCellAsUint32(size_t colIdx, size_t rowIdx, uint32 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    uint64 rawData;
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
ArrowChunkIterator::getCellAsUint64(size_t colIdx, size_t rowIdx, uint64 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

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
ArrowChunkIterator::getCellAsFloat32(size_t colIdx, size_t rowIdx, float32 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

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
ArrowChunkIterator::getCellAsFloat64(size_t colIdx, size_t rowIdx, float64 * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

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

SF_STATUS STDCALL ArrowChunkIterator::getCellAsString(
    size_t colIdx,
    size_t rowIdx,
    std::string& outString
)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    if (isCellNull(colData, arrowType, cellIdx))
    {
        outString = "";
        return SF_STATUS_SUCCESS;
    }

    SF_DB_TYPE snowType = m_metadata[colIdx].type;
    int64 scale = m_metadata[colIdx].scale;
    switch (arrowType->id())
    {
        case arrow::Type::type::BINARY:
            return Conversion::Arrow::BinaryToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, outString);
        case arrow::Type::type::BOOL:
            return Conversion::Arrow::BoolToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, outString);
        case arrow::Type::type::DATE32:
        case arrow::Type::type::DATE64:
            return Conversion::Arrow::DateToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, outString);
        case arrow::Type::type::DECIMAL:
            return Conversion::Arrow::DecimalToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, outString);
        case arrow::Type::type::DOUBLE:
            return Conversion::Arrow::DoubleToString(
                colData, arrowType, colIdx, rowIdx, cellIdx, outString);
        case arrow::Type::type::INT8:
        case arrow::Type::type::INT16:
        case arrow::Type::type::INT32:
        case arrow::Type::type::INT64:
            return Conversion::Arrow::IntToString(
                colData, arrowType, snowType, scale,
                colIdx, rowIdx, cellIdx, outString);
        case arrow::Type::type::STRUCT:
            return Conversion::Arrow::TimestampToString(
                colData, arrowType, snowType, scale,
                colIdx, rowIdx, cellIdx, outString);
        case arrow::Type::type::STRING:
        {
            outString = colData->arrowString->GetString(cellIdx);
            return SF_STATUS_SUCCESS;
        }
        default:
            CXX_LOG_ERROR("Unsupported conversion from %d to STRING.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsTimestamp(size_t colIdx, size_t rowIdx, SF_TIMESTAMP * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
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
        return snowflake_timestamp_from_parts(out_data, 0, 0, 0, 0, 1, 1, 1970, 0, 9, SF_DB_TYPE_TIMESTAMP_NTZ);
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
ArrowChunkIterator::getCellStrlen(size_t colIdx, size_t rowIdx, size_t * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);

    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Static buffers to hold fixed-size values for performance.
    // uint8_t binBuf[128];

    std::shared_ptr<arrow::DataType> arrowType = colData->type;
    switch(arrowType->id())
    {
        case arrow::Type::type::BINARY:
        {
            break;
        }
        case arrow::Type::type::BOOL:
        {
            break;
        }
        case arrow::Type::type::DATE32:
        {
            auto rawData = colData->arrowDate32->Value(cellIdx);
            *out_data = std::trunc(std::log10(rawData)) + 1;
            break;
        }
        case arrow::Type::type::DATE64:
        {
            auto rawData = colData->arrowDate64->Value(cellIdx);
            *out_data = std::trunc(std::log10(rawData)) + 1;
            break;
        }
        case arrow::Type::type::DECIMAL:
        {
            break;
        }
        case arrow::Type::type::DOUBLE:
        {
            auto rawData = colData->arrowDouble->Value(cellIdx);
            *out_data = std::to_string(rawData).size();
            break;
        }
        case arrow::Type::type::INT8:
        {
            auto rawData = colData->arrowInt8->Value(cellIdx);
            *out_data = std::trunc(std::log10(rawData)) + 1;
            break;
        }
        case arrow::Type::type::INT16:
        {
            auto rawData = colData->arrowInt16->Value(cellIdx);
            *out_data = std::trunc(std::log10(rawData)) + 1;
            break;
        }
        case arrow::Type::type::INT32:
        {
            auto rawData = colData->arrowInt32->Value(cellIdx);
            *out_data = std::trunc(std::log10(rawData)) + 1;
            break;
        }
        case arrow::Type::type::INT64:
        {
            auto rawData = colData->arrowInt64->Value(cellIdx);
            *out_data = std::trunc(std::log10(rawData)) + 1;
            break;
        }
        case arrow::Type::type::STRING:
        {
            break;
        }
        case arrow::Type::STRUCT:
        {
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

std::shared_ptr<arrow::DataType> ArrowChunkIterator::getColumnArrowDataType(uint32 colIdx)
{
    return m_schema->field(colIdx)->type();
}

size_t ArrowChunkIterator::getRowCountInChunk()
{
    return m_rowCountInChunk;
}

SF_STATUS STDCALL
ArrowChunkIterator::isCellNull(size_t colIdx, size_t rowIdx, sf_bool * out_data)
{
    int32 cellIdx;
    std::shared_ptr<ArrowColumn> colData = getColumn(colIdx, rowIdx, &cellIdx);
    if (colData == nullptr)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    CXX_LOG_TRACE("cellIdx is %d", cellIdx);
    std::shared_ptr<arrow::DataType> arrowType = getColumnArrowDataType(colIdx);

    switch (arrowType->id())
    {
        case arrow::Type::type::BINARY:
        {
            *out_data = colData->arrowBinary->IsNull(cellIdx) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            break;
        }
        case arrow::Type::type::BOOL:
        {
            CXX_LOG_TRACE("Bool value IsNull? %d", colData->arrowBoolean->IsNull(cellIdx));
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
    size_t colIdx,
    size_t rowIdx,
    int32 * out_cellIdx
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
            *out_cellIdx = rowIdx - prevMaxRowIdx;

            // Since there is one set of columns stored for each batch, simply
            // multiply batch index with column count and add the given column index.
            uint32 trueColIdx = m_columnCount * batchRowIdxPair.first + colIdx;
            return m_columns[trueColIdx];
        }
        else
        {
            prevMaxRowIdx = batchRowIdxPair.second;
        }
    }

    // Should never reach.
    CXX_LOG_ERROR("Could not find the appropriate column element.");
    return nullptr;
}

bool ArrowChunkIterator::isCellNull(
    std::shared_ptr<ArrowColumn> colData,
    std::shared_ptr<arrow::DataType> arrowType,
    int32 cellIdx
)
{
    switch (arrowType->id())
    {
        case arrow::Type::type::BINARY:
            return colData->arrowBinary->IsNull(cellIdx);
        case arrow::Type::type::BOOL:
            return colData->arrowBoolean->IsNull(cellIdx);
        case arrow::Type::type::DATE32:
            return colData->arrowDate32->IsNull(cellIdx);
        case arrow::Type::type::DATE64:
            return colData->arrowDate64->IsNull(cellIdx);
        case arrow::Type::type::DECIMAL:
            return colData->arrowDecimal128->IsNull(cellIdx);
        case arrow::Type::type::DOUBLE:
            return colData->arrowDouble->IsNull(cellIdx);
        case arrow::Type::type::INT8:
            return colData->arrowInt8->IsNull(cellIdx);
        case arrow::Type::type::INT16:
            return colData->arrowInt16->IsNull(cellIdx);
        case arrow::Type::type::INT32:
            return colData->arrowInt32->IsNull(cellIdx);
        case arrow::Type::type::INT64:
            return colData->arrowInt64->IsNull(cellIdx);
        case arrow::Type::type::STRING:
            return colData->arrowString->IsNull(cellIdx);
        case arrow::Type::STRUCT:
            return colData->arrowTimestamp->sse->IsNull(cellIdx);
        default:
            CXX_LOG_ERROR("Unsupported Arrow type: %d.", arrowType->id());
            return true;
    }
}

} // namespace Client
} // namespace Snowflake
#endif // ifndef SF_WIN32
