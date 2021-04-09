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
#include "ResultSetArrow.hpp"

#ifndef SF_WIN32
#undef BOOL

namespace Snowflake
{
namespace Client
{

ArrowChunkIterator::ArrowChunkIterator(arrow::BufferBuilder * chunk,
                                       SF_COLUMN_DESC * metadata, std::string tzString,
                                       ResultSetArrow * parent)
    : m_metadata(metadata), m_tzString(tzString), m_parent(parent)
{
    // Finalize the currently built buffer and initialize reader objects from it.
    // This resets the buffer builder for future use.
    std::shared_ptr<arrow::Buffer> arrowResultData;
    (void) chunk->Finish(&arrowResultData);
    std::shared_ptr<arrow::io::BufferReader> bufferReader =
        std::make_shared<arrow::io::BufferReader>(arrowResultData);

    // Add the batches to a vector.
    // This stores batches as they are retrieved from the server row-wise
    // and passes them to the ArrowChunkIterator.
    arrow::Result<std::shared_ptr<arrow::ipc::RecordBatchReader>> batchReader =
        arrow::ipc::RecordBatchStreamReader::Open(std::move(bufferReader));

    while (true)
    {
        std::shared_ptr<arrow::RecordBatch> batch;
        (void) batchReader.ValueOrDie()->ReadNext(&batch);
        if (batch == nullptr)
            break;
        m_cRecordBatches.emplace_back(batch);
    }

    // delete the original BufferBuilder since the buffer is passed to batches already
    delete chunk;

    m_batchCount = m_cRecordBatches.size();
    m_columnCount = m_batchCount > 0 ? (m_cRecordBatches)[0]->num_columns() : 0;
    m_rowCountInBatch = m_batchCount > 0 ? (m_cRecordBatches)[0]->num_rows() : 0;
    m_currentSchema = m_batchCount > 0 ? (m_cRecordBatches)[0]->schema() : nullptr;
    m_currBatchIndex = 0;
    m_currRowIndexInBatch = -1;

    for (int col = 0; col < m_columnCount; ++col) {
        m_arrowColumnDataTypes.push_back(m_currentSchema->field(col)->type()->id());
        CXX_LOG_TRACE("ArrowChunkIterator: col:%d arrow::Type: %s",
            col, m_currentSchema->field(col)->type()->name().c_str());
    }
}

// Public methods ==================================================================================
bool ArrowChunkIterator::next()
{
    m_currRowIndexInBatch++;

    //If its the first row in the batch then initialize the new set of columns.
    if (m_columns.size() == 0)
        this->initColumnChunks();

    if (m_currRowIndexInBatch < m_rowCountInBatch)
    {
        return true;
    }
    else
    {
        CXX_LOG_TRACE("ArrowChunkIterator: recordBatch %d with %ld rows.",
            m_currBatchIndex, m_rowCountInBatch);
        m_currBatchIndex++;
        if (m_currBatchIndex < m_batchCount)
        {
            m_currRowIndexInBatch = 0;
            m_rowCountInBatch = (m_cRecordBatches)[m_currBatchIndex]->num_rows();
            CXX_LOG_TRACE("ArrowChunkIterator: Initiating record batch %d with %ld rows.",
                m_currBatchIndex, m_rowCountInBatch);
            this->initColumnChunks();
            return true;
        }
    }
    return false;
}

bool ArrowChunkIterator::isCellNull(int32 col)
{
    switch (m_arrowColumnDataTypes[col])
    {
    case (arrow::Type::type::DATE32):
    {
        return m_columns[col].arrowDate32->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::DATE64):
    {
        return m_columns[col].arrowDate64->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::STRING):
    {
        return m_columns[col].arrowString->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::INT8):
    {
        return m_columns[col].arrowInt8->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::INT16):
    {
        return m_columns[col].arrowInt16->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::INT32):
    {
        return m_columns[col].arrowInt32->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::INT64):
    {
        return m_columns[col].arrowInt64->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::BOOL):
    {
        return m_columns[col].arrowBoolean->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::BINARY):
    {
        return m_columns[col].arrowBinary->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::DOUBLE):
    {
        return m_columns[col].arrowDouble->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::DECIMAL):
    {
        return m_columns[col].arrowDecimal128->IsNull(m_currRowIndexInBatch);
        break;
    }
    case (arrow::Type::type::STRUCT):
    {
        return (m_columns[col].arrowTimestamp->sse->IsNull(m_currRowIndexInBatch));
        break;
    }
    default:
        break;
    }
    return false;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsBool(size_t colIdx, sf_bool * out_data)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    *out_data = SF_BOOLEAN_FALSE;

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    switch (m_arrowColumnDataTypes[colIdx])
    {
        case arrow::Type::type::BOOL:
        {
            auto rawData = m_columns[colIdx].arrowBoolean->Value(m_currRowIndexInBatch);
            *out_data = (rawData) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::DOUBLE:
        {
            auto rawData = m_columns[colIdx].arrowDouble->Value(m_currRowIndexInBatch);
            *out_data = (rawData == 0.0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::INT8:
        {
            auto rawData = m_columns[colIdx].arrowInt8->Value(m_currRowIndexInBatch);
            *out_data = (rawData == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::INT16:
        {
            auto rawData = m_columns[colIdx].arrowInt16->Value(m_currRowIndexInBatch);
            *out_data = (rawData == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::INT32:
        {
            auto rawData = m_columns[colIdx].arrowInt32->Value(m_currRowIndexInBatch);
            *out_data = (rawData == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::INT64:
        {
            auto rawData = m_columns[colIdx].arrowInt64->Value(m_currRowIndexInBatch);
            *out_data = (rawData == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case arrow::Type::type::STRING:
        {
            auto rawData = m_columns[colIdx].arrowString->GetString(m_currRowIndexInBatch);
            *out_data = (rawData.empty()) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        default:
        {
            CXX_LOG_ERROR("Unsupported conversion from %d to BOOLEAN.", m_arrowColumnDataTypes[colIdx]);
            m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                "No valid conversion to boolean from data type.");
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsInt8(size_t colIdx, int8 * out_data)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    *out_data = 0;

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    std::string strVal;
    SF_STATUS status = getCellAsString(colIdx, strVal);
    if (status != SF_STATUS_SUCCESS)
    {
        return status;
    }

    if (strVal.empty())
    {
        *out_data = 0;
    }
    else
    {
        *out_data = (int8)strVal[0];
    }

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsInt32(size_t colIdx, int32 * out_data)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    *out_data = 0;

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    // Not go through conversion if type matched for performance.
    if ((arrow::Type::type::INT32 == m_arrowColumnDataTypes[colIdx]) &&
        ((SF_DB_TYPE_FIXED != m_metadata[colIdx].type) || (0 == m_metadata[colIdx].scale)))
    {
        *out_data = m_columns[colIdx].arrowInt32->Value(m_currRowIndexInBatch);
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    int64 rawData;

    status = getCellAsInt64(colIdx, &rawData);
    if (status != SF_STATUS_SUCCESS)
    {
        return status;
    }

    status = Conversion::Arrow::IntegerToInteger(rawData, &rawData, INT32);
    if (status != SF_STATUS_SUCCESS)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
            "Value out of range for int32.");
        return status;
    }

    *out_data = static_cast<int32>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsInt64(size_t colIdx, int64 * out_data, bool rawData)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    *out_data = 0;

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    // Not go through conversion if type matched for performance.
    if ((arrow::Type::type::INT64 == m_arrowColumnDataTypes[colIdx]) &&
        ((SF_DB_TYPE_FIXED != m_metadata[colIdx].type) || (0 == m_metadata[colIdx].scale) || rawData))
    {
        *out_data = m_columns[colIdx].arrowInt64->Value(m_currRowIndexInBatch);
        return SF_STATUS_SUCCESS;
    }

    if ((!rawData) && (SF_DB_TYPE_FIXED == m_metadata[colIdx].type) && (m_metadata[colIdx].scale != 0))
    {
        float64 floatData;
        SF_STATUS status = getCellAsFloat64(colIdx, &floatData);
        if (SF_STATUS_SUCCESS != status)
        {
            m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                "Cannot convert value to int64.");
            return status;
        }

        if ((floatData > SF_INT64_MAX) || (floatData < SF_INT64_MIN))
        {
            m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Value out of range for int64.");
            return SF_STATUS_ERROR_OUT_OF_RANGE;
        }

        *out_data = (int64)floatData;
        return SF_STATUS_SUCCESS;
    }

    int64 data;
    switch (m_arrowColumnDataTypes[colIdx])
    {
        case arrow::Type::type::BOOL:
        {
            data = (int64)m_columns[colIdx].arrowBoolean->Value(m_currRowIndexInBatch);
            break;
        }
        case arrow::Type::type::DATE32:
        {
            data = (int64)m_columns[colIdx].arrowDate32->Value(m_currRowIndexInBatch);
            break;
        }
        case arrow::Type::type::DATE64:
        {
            data = (int64)m_columns[colIdx].arrowDate64->Value(m_currRowIndexInBatch);
            break;
        }
        case arrow::Type::type::INT8:
        {
            data = (int64)m_columns[colIdx].arrowInt8->Value(m_currRowIndexInBatch);
            break;
        }
        case arrow::Type::type::INT16:
        {
            data = (int64)m_columns[colIdx].arrowInt16->Value(m_currRowIndexInBatch);
            break;
        }
        case arrow::Type::type::INT32:
        {
            data = (int64)m_columns[colIdx].arrowInt32->Value(m_currRowIndexInBatch);
            break;
        }
        case arrow::Type::type::DOUBLE:
        {
            double dblData = m_columns[colIdx].arrowDouble->Value(m_currRowIndexInBatch);
            if ((dblData > SF_INT64_MAX) || (dblData < SF_INT64_MIN))
            {
                m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                    "Value out of range for int64.");
                return SF_STATUS_ERROR_OUT_OF_RANGE;
            }
            data = (int64)dblData;
            break;
        }
        case arrow::Type::type::DECIMAL:
        {
            std::string strData = m_columns[colIdx].arrowDecimal128->FormatValue(m_currRowIndexInBatch);
            return Conversion::Arrow::StringToInteger(strData, out_data, INT64);
        }
        case arrow::Type::type::STRING:
        {
            std::string strData = m_columns[colIdx].arrowString->GetString(m_currRowIndexInBatch);
            return Conversion::Arrow::StringToInteger(strData, out_data, INT64);
        }
        default:
        {
            CXX_LOG_ERROR("Unsupported conversion from %d to INT64.", m_arrowColumnDataTypes[colIdx]);
            m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                "Cannot convert value to int64.");
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    *out_data = data;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsUint8(size_t colIdx, uint8 * out_data)
{
    return getCellAsInt8(colIdx, (int8*)out_data);
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsUint32(size_t colIdx, uint32 * out_data)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    *out_data = 0;

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    // Not go through conversion if type matched for performance.
    if ((arrow::Type::type::INT32 == m_arrowColumnDataTypes[colIdx]) &&
        ((SF_DB_TYPE_FIXED != m_metadata[colIdx].type) || (0 == m_metadata[colIdx].scale)))
    {
        int32 data = m_columns[colIdx].arrowInt32->Value(m_currRowIndexInBatch);
        *out_data = (uint32)data;
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    int64 rawData;

    status = getCellAsInt64(colIdx, &rawData);
    if (status != SF_STATUS_SUCCESS)
    {
        return status;
    }

    status = Conversion::Arrow::IntegerToInteger(rawData, &rawData, UINT32);
    if (status != SF_STATUS_SUCCESS)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
            "Value out of range for uint32.");
    }

    *out_data = static_cast<int32>(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsUint64(size_t colIdx, uint64 * out_data)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    *out_data = 0;

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    int64 data;
    // Not go through conversion if type matched for performance.
    if ((arrow::Type::type::INT64 == m_arrowColumnDataTypes[colIdx]) &&
        ((SF_DB_TYPE_FIXED != m_metadata[colIdx].type) || (0 == m_metadata[colIdx].scale)))
    {
        data = m_columns[colIdx].arrowInt64->Value(m_currRowIndexInBatch);
        *out_data = (uint64)data;
        return SF_STATUS_SUCCESS;
    }

    switch (m_arrowColumnDataTypes[colIdx])
    {
    case arrow::Type::type::BOOL:
    case arrow::Type::type::DATE32:
    case arrow::Type::type::DATE64:
    case arrow::Type::type::INT8:
    case arrow::Type::type::INT16:
    case arrow::Type::type::INT32:
    {
        status = getCellAsInt64(colIdx, &data);
        if (status)
        {
            return status;
        }
        *out_data = (uint64)data;
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::DOUBLE:
    {
        double dblData = m_columns[colIdx].arrowDouble->Value(m_currRowIndexInBatch);
        if ((dblData > SF_UINT64_MAX) || (dblData < SF_INT64_MIN))
        {
            m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Value out of range for uint64.");
            return SF_STATUS_ERROR_OUT_OF_RANGE;
        }
        *out_data = (uint64)dblData;
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::DECIMAL:
    {
        std::string strData = m_columns[colIdx].arrowDecimal128->FormatValue(m_currRowIndexInBatch);
        status = Conversion::Arrow::StringToUint64(strData, out_data);
        break;
    }
    case arrow::Type::type::STRING:
    {
        std::string strData = m_columns[colIdx].arrowString->GetString(m_currRowIndexInBatch);
        status = Conversion::Arrow::StringToUint64(strData, out_data);
        break;
    }
    default:
    {
        CXX_LOG_ERROR("Unsupported conversion from %d to UINT64.", m_arrowColumnDataTypes[colIdx]);
        m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "No valid conversion to uint64 from data type.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
    }

    if (SF_STATUS_SUCCESS != status)
    {
        if (SF_STATUS_ERROR_OUT_OF_RANGE == status)
        {
            m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Value out of range for uint64.");
        }
        else
        {
            m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                "Cannot convert value to uint64.");
        }
    }

    return status;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsFloat32(size_t colIdx, float32 * out_data)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    *out_data = 0.0;

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    // Not go through conversion if type matched for performance.
    if (arrow::Type::type::DOUBLE == m_arrowColumnDataTypes[colIdx])
    {
        float64 floatValue = m_columns[colIdx].arrowDouble->Value(m_currRowIndexInBatch);
        if (floatValue == INFINITY || floatValue == -INFINITY)
        {
            m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Value out of range for float32.");
            return SF_STATUS_ERROR_OUT_OF_RANGE;
        }

        *out_data = (float32)floatValue;
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    switch (m_arrowColumnDataTypes[colIdx])
    {
    case arrow::Type::type::BOOL:
    case arrow::Type::type::DATE32:
    case arrow::Type::type::DATE64:
    case arrow::Type::type::INT8:
    case arrow::Type::type::INT16:
    case arrow::Type::type::INT32:
    case arrow::Type::type::INT64:
    {
        float64 data;
        status = getCellAsFloat64(colIdx, &data);
        if (status)
        {
            return status;
        }
        *out_data = (float32)data;
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::DECIMAL:
    {
        std::string strData = m_columns[colIdx].arrowDecimal128->FormatValue(m_currRowIndexInBatch);
        status = Conversion::Arrow::StringToFloat(strData, out_data);
        break;
    }
    case arrow::Type::type::STRING:
    {
        std::string strData = m_columns[colIdx].arrowString->GetString(m_currRowIndexInBatch);
        status = Conversion::Arrow::StringToFloat(strData, out_data);
        break;
    }
    default:
    {
        CXX_LOG_ERROR("Unsupported conversion from %d to FLOAT32.", m_arrowColumnDataTypes[colIdx]);
        m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "No valid conversion to float32 from data type.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
    }

    if (SF_STATUS_SUCCESS != status)
    {
        if (SF_STATUS_ERROR_OUT_OF_RANGE == status)
        {
            m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Value out of range for float32.");
        }
        else
        {
            m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                "Cannot convert value to float32.");
        }
    }

    return status;
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsFloat64(size_t colIdx, float64 * out_data)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    *out_data = 0.0;

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    // Not go through conversion if type matched for performance.
    if (arrow::Type::type::DOUBLE == m_arrowColumnDataTypes[colIdx])
    {
        float64 floatValue = m_columns[colIdx].arrowDouble->Value(m_currRowIndexInBatch);
        if (floatValue == INFINITY || floatValue == -INFINITY)
        {
            m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Value out of range for float64.");
            return SF_STATUS_ERROR_OUT_OF_RANGE;
        }

        *out_data = floatValue;
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;
    switch (m_arrowColumnDataTypes[colIdx])
    {
    case arrow::Type::type::BOOL:
    case arrow::Type::type::DATE32:
    case arrow::Type::type::DATE64:
    case arrow::Type::type::INT8:
    case arrow::Type::type::INT16:
    case arrow::Type::type::INT32:
    case arrow::Type::type::INT64:
    {
        int64 intData;
        status = getCellAsInt64(colIdx, &intData, true);
        if (status)
        {
            return status;
        }

        float64 floatData = (float64)intData;
        if ((SF_DB_TYPE_FIXED == m_metadata[colIdx].type) && (m_metadata[colIdx].scale > 0))
        {
            floatData /= (float64)(power10[m_metadata[colIdx].scale]);
        }
        *out_data = floatData;
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::DECIMAL:
    {
        std::string strData = m_columns[colIdx].arrowDecimal128->FormatValue(m_currRowIndexInBatch);
        status = Conversion::Arrow::StringToDouble(strData, out_data);
        break;
    }
    case arrow::Type::type::STRING:
    {
        std::string strData = m_columns[colIdx].arrowString->GetString(m_currRowIndexInBatch);
        status = Conversion::Arrow::StringToDouble(strData, out_data);
        break;
    }
    default:
    {
        CXX_LOG_ERROR("Unsupported conversion from %d to FLOAT64.", m_arrowColumnDataTypes[colIdx]);
        m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "No valid conversion to float64 from data type.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
    }

    if (SF_STATUS_SUCCESS != status)
    {
        if (SF_STATUS_ERROR_OUT_OF_RANGE == status)
        {
            m_parent->setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Value out of range for float64.");
        }
        else
        {
            m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                "Cannot convert value to float64.");
        }
    }

    return status;
}

SF_STATUS STDCALL ArrowChunkIterator::getCellAsString(
    size_t colIdx,
    std::string& outString
)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // Set default value for null and error cases.
    outString = "";

    if (isCellNull(colIdx))
    {
        return SF_STATUS_SUCCESS;
    }

    SF_STATUS status;

    SF_DB_TYPE snowType = m_metadata[colIdx].type;

    if ((SF_DB_TYPE_TIMESTAMP_TZ == snowType) ||
        (SF_DB_TYPE_TIMESTAMP_NTZ == snowType) ||
        (SF_DB_TYPE_TIMESTAMP_LTZ == snowType))
    {
        SF_TIMESTAMP ts;
        status = getCellAsTimestamp(colIdx, &ts);
        if (SF_STATUS_SUCCESS != status)
        {
            return status;
        }

        char buf[64];
        char* bufPtr = buf;
        status = snowflake_timestamp_to_string(&ts, "", &bufPtr, sizeof(buf), NULL, SF_BOOLEAN_FALSE);
        if (SF_STATUS_SUCCESS != status)
        {
            m_parent->setError(status, "Failed to convert value to string.");
            return status;
        }

        outString = std::string(buf);
        return SF_STATUS_SUCCESS;
    }

    switch (m_arrowColumnDataTypes[colIdx])
    {
    case arrow::Type::type::STRING:
    {
        outString = m_columns[colIdx].arrowString->GetString(m_currRowIndexInBatch);
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::BINARY:
    {
        int len = 0;
        auto values = m_columns[colIdx].arrowBinary->GetValue(m_currRowIndexInBatch, &len);
        std::vector<char> buffer(len * 2 + 1);
        char* ptr = buffer.data();
        for (int i = 0; i < len; i++)
        {
            sprintf(ptr, "%02X", values[i]);
            ptr += 2;
        }
        *ptr = '\0';
        outString = std::string(buffer.data());
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::BOOL:
    {
        outString = m_columns[colIdx].arrowBoolean->Value(m_currRowIndexInBatch) ?
                        SF_BOOLEAN_TRUE_STR : SF_BOOLEAN_FALSE_STR;
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::INT8:
    case arrow::Type::type::INT16:
    case arrow::Type::type::INT32:
    case arrow::Type::type::INT64:
    {
        if ((SF_DB_TYPE_FIXED == snowType) && (m_metadata[colIdx].scale != 0))
        {
            float64 floatData;
            status = getCellAsFloat64(colIdx, &floatData);
            if (SF_STATUS_SUCCESS != status)
            {
                return status;
            }
            outString = std::to_string(floatData);
            return SF_STATUS_SUCCESS;
        }

        int64 data;
        status = getCellAsInt64(colIdx, &data);
        if (SF_STATUS_SUCCESS != status)
        {
            return status;
        }

        // INT32 and INT64 values may contain Snowflake TIME/TIMESTAMP values.
        if (SF_DB_TYPE_TIME == snowType)
        {
            status = Conversion::Arrow::TimeToString(data, m_metadata[colIdx].scale, outString);
            if (SF_STATUS_SUCCESS != status)
            {
                m_parent->setError(status, "Failed to convert value to string.");
            }
            return status;
        }

        outString = std::to_string(data);
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::DATE32:
    case arrow::Type::type::DATE64:
    {
        int64 date;
        status = getCellAsInt64(colIdx, &date);
        if (SF_STATUS_SUCCESS != status)
        {
            return status;
        }

        status = Conversion::Arrow::DateToString(date, outString);
        if (SF_STATUS_SUCCESS != status)
        {
            m_parent->setError(status, "Failed to convert value to string.");
        }
        return status;
    }
    case arrow::Type::type::DOUBLE:
    {
        outString = std::to_string(m_columns[colIdx].arrowDouble->Value(m_currRowIndexInBatch));
        // remove trailing 0
        outString.erase(outString.find_last_not_of('0') + 1, std::string::npos);
        return SF_STATUS_SUCCESS;
    }
    case arrow::Type::type::DECIMAL:
    {
        outString = m_columns[colIdx].arrowDecimal128->FormatValue(m_currRowIndexInBatch);
        return SF_STATUS_SUCCESS;
    }
    default:
    {
        CXX_LOG_ERROR("Unsupported conversion from %d to STRING.", m_arrowColumnDataTypes[colIdx]);
        m_parent->setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "No valid conversion to string from data type.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
    }
}

SF_STATUS STDCALL
ArrowChunkIterator::getCellAsTimestamp(size_t colIdx, SF_TIMESTAMP * out_data)
{
    if (colIdx >= m_columnCount)
    {
        m_parent->setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (isCellNull(colIdx))
    {
        return snowflake_timestamp_from_parts(out_data, 0, 0, 0, 0, 1, 1, 1970, 0, 9, SF_DB_TYPE_TIMESTAMP_NTZ);
    }

    SF_DB_TYPE snowType = m_metadata[colIdx].type;
    uint8 arrowType = m_arrowColumnDataTypes[colIdx];
    SF_STATUS status;

    if ((SF_DB_TYPE_TIMESTAMP_TZ != snowType) &&
        (SF_DB_TYPE_TIMESTAMP_NTZ != snowType) &&
        (SF_DB_TYPE_TIMESTAMP_LTZ != snowType) &&
        (SF_DB_TYPE_TIME != snowType) &&
        (SF_DB_TYPE_DATE != snowType))
    {
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    int64 secondsSinceEpoch = 0;
    int64 fracSeconds = 0;
    int32 tz = 0;
    int64 scale = m_metadata[colIdx].scale;

    if (arrowType != arrow::Type::type::STRUCT)
    {
        status = getCellAsInt64(colIdx, &fracSeconds);
        if (SF_STATUS_SUCCESS != status)
        {
            return status;
        }
    }
    else
    {
        if (m_columns[colIdx].arrowTimestamp->sse)
            secondsSinceEpoch = m_columns[colIdx].arrowTimestamp->sse->Value(m_currRowIndexInBatch);
        if (m_columns[colIdx].arrowTimestamp->fs)
            fracSeconds = m_columns[colIdx].arrowTimestamp->fs->Value(m_currRowIndexInBatch);
        if (m_columns[colIdx].arrowTimestamp->tz)
            tz = m_columns[colIdx].arrowTimestamp->tz->Value(m_currRowIndexInBatch);

        if (SF_DB_TYPE_TIMESTAMP_TZ == snowType && !m_columns[colIdx].arrowTimestamp->tz)
        {
            // should have timezone information but it's null.
            // fracSeconds actually is timezone information and fracSeconds is included in secondsSinceEpoch
            tz = fracSeconds;
            fracSeconds = secondsSinceEpoch;
            secondsSinceEpoch = 0;
        }
        else
        {
            // If we get fraction from struct, it's in nanosec so the scale is 9.
            scale = 9;
        }
    }

    if (secondsSinceEpoch == 0)
    {
        secondsSinceEpoch = fracSeconds / power10[scale];
        fracSeconds = fracSeconds % power10[scale];
    }

    bool needMinus = false;
    if (fracSeconds < 0)
    {
        if (secondsSinceEpoch <= 0)
        {
            fracSeconds = -fracSeconds;
            if (0 == secondsSinceEpoch)
            {
                // when second part is 0 and fraction is nagative, add '-' at beginning.
                needMinus = true;
            }
        }
        else if (secondsSinceEpoch > 0)
        {
            fracSeconds += power10[scale];
            secondsSinceEpoch--;
        }
    }
    else if (fracSeconds > 0)
    {
        if (secondsSinceEpoch < 0)
        {
            fracSeconds = power10[scale] - fracSeconds;
            secondsSinceEpoch++;
        }
    }
    else
    {
        scale = 0;
    }

    // covert fracSeconds to nsec
    if (scale)
    {
        fracSeconds *= power10[9 - scale];
        scale = 9;
    }

    char buf[64];
    std::string tsString;
    if (needMinus)
    {
        tsString = "-";
    }
    tsString += std::to_string(secondsSinceEpoch);

    if (fracSeconds)
    {
        sb_sprintf(buf, sizeof(buf), ".%09d", (uint32)fracSeconds);
        tsString += std::string(buf);
    }
    if (tz)
    {
        sb_sprintf(buf, sizeof(buf), " %d", tz);
        tsString += std::string(buf);
    }

    status = snowflake_timestamp_from_epoch_seconds(out_data, tsString.c_str(), m_tzString.c_str(), scale, snowType);
    if (SF_STATUS_SUCCESS != status)
    {
        return status;
    }

    out_data->scale = m_metadata[colIdx].scale;
    return SF_STATUS_SUCCESS;
}

size_t ArrowChunkIterator::getRowCountInChunk()
{
    size_t rowCount = 0;
    for (unsigned int i = 0; i < (m_cRecordBatches).size(); ++i) {
        size_t rowCountBatch = (m_cRecordBatches)[i]->num_rows();
        rowCount += rowCountBatch;
    }
    return rowCount;
}

// Private methods =================================================================================
void ArrowChunkIterator::initColumnChunks()
{
    m_columns.clear();

    ArrowColumn arrowcol;

    std::shared_ptr<arrow::RecordBatch> currentBatch = (m_cRecordBatches)[m_currBatchIndex];

    m_currentSchema = currentBatch->schema();
    m_rowCountInBatch = currentBatch->num_rows();
    for (int i = 0; i < m_columnCount; i++)
    {
        std::shared_ptr<arrow::Array> columnArray = currentBatch->column(i);
        std::shared_ptr<arrow::DataType> dt = m_currentSchema->field(i)->type();

        switch (dt->id())
        {
            case arrow::Type::STRUCT: {
                auto values = std::static_pointer_cast<arrow::StructArray>(columnArray);
                std::shared_ptr<ArrowTimestampArray> ts(new ArrowTimestampArray);
                ts->sse = std::static_pointer_cast<arrow::Int64Array>(values->field(0)).get();
                if (values->num_fields() > 1)
                    ts->fs = std::static_pointer_cast<arrow::Int32Array>(values->field(1)).get();
                if (values->num_fields() > 2)
                    ts->tz = std::static_pointer_cast<arrow::Int32Array>(values->field(2)).get();
                arrowcol.arrowTimestamp = ts;
                m_columns.emplace_back(arrowcol);
                break;
            }

            case arrow::Type::type::DATE32: {
                arrowcol.arrowDate32 = std::static_pointer_cast<arrow::Date32Array>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::DATE64: {
                arrowcol.arrowDate64 = std::static_pointer_cast<arrow::Date64Array>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::STRING: {
                arrowcol.arrowString = std::static_pointer_cast<arrow::StringArray>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::INT8: {
                arrowcol.arrowInt8 = std::static_pointer_cast<arrow::Int8Array>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::INT16: {
                arrowcol.arrowInt16 = std::static_pointer_cast<arrow::Int16Array>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::INT32: {
                arrowcol.arrowInt32 = std::static_pointer_cast<arrow::Int32Array>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::INT64: {
                arrowcol.arrowInt64 = std::static_pointer_cast<arrow::Int64Array>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::BOOL: {
                arrowcol.arrowBoolean = std::static_pointer_cast<arrow::BooleanArray>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::BINARY: {
                arrowcol.arrowBinary = std::static_pointer_cast<arrow::BinaryArray>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::DOUBLE: {
                arrowcol.arrowDouble = std::static_pointer_cast<arrow::DoubleArray>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }
            case arrow::Type::type::DECIMAL:{
                arrowcol.arrowDecimal128 = std::static_pointer_cast<arrow::Decimal128Array>(columnArray).get();
                m_columns.emplace_back(arrowcol);
                break;
            }

            default:
            {
                return;
            }
        }
    }
}
} // namespace Client
} // namespace Snowflake
#endif // ifndef SF_WIN32
