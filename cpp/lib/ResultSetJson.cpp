/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include <cerrno>
#include <cstring>
#include <stdlib.h>

#include "../logger/SFLogger.hpp"
#include "ResultSetJson.hpp"

namespace Snowflake
{
namespace Client
{


ResultSetJson::ResultSetJson() :
    ResultSet()
{
    m_queryResultFormat = QueryResultFormat::JSON;
}

ResultSetJson::ResultSetJson(
    cJSON * initialChunk,
    SF_COLUMN_DESC * metadata,
    std::string tzString
) :
    ResultSet(metadata, tzString)
{
    m_queryResultFormat = QueryResultFormat::JSON;
    m_totalColumnCount = snowflake_cJSON_GetArraySize(
        snowflake_cJSON_GetArrayItem(initialChunk, 0));
    m_records = snowflake_cJSON_CreateArray();
    this->appendChunk(initialChunk);
}

ResultSetJson::~ResultSetJson()
{
    snowflake_cJSON_Delete(m_records);
}

// Public methods ==================================================================================

SF_STATUS STDCALL ResultSetJson::appendChunk(cJSON * chunk)
{
    if (chunk == nullptr)
    {
        CXX_LOG_ERROR("appendChunk -- Received a null chunk to append.");
        return SF_STATUS_ERROR_NULL_POINTER;
    }

    if (!snowflake_cJSON_IsArray(chunk))
    {
        CXX_LOG_ERROR("appendChunk -- Given chunk is not of type array.");
        return SF_STATUS_ERROR_OTHER;
    }

    snowflake_cJSON_AddItemToArray(m_records, chunk->child);
    m_rowCountInChunk = snowflake_cJSON_GetArraySize(chunk);
    m_totalChunkCount++;
    m_totalRowCount += m_rowCountInChunk;
    CXX_LOG_DEBUG("appendChunk -- Appended chunk of size %d.", m_rowCountInChunk, chunk->child);

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::finishResultSet()
{
    m_currChunkIdx = 0;
    m_currChunkRowIdx = 0;
    m_currRowIdx = 0;

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::next()
{
    if (m_currColumnIdx == m_totalColumnCount - 1)
    {
        if (m_currRowIdx == m_totalRowCount - 1)
        {
            CXX_LOG_DEBUG("next -- Already advanced to end of result set.");
            return SF_STATUS_ERROR_OUT_OF_BOUNDS;
        }

        m_currRowIdx++;
        m_currColumnIdx = 0;
        return SF_STATUS_SUCCESS;
    }

    m_currColumnIdx++;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsBool(sf_bool * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        *out_data = SF_BOOLEAN_FALSE;
        return SF_STATUS_SUCCESS;
    }

    char * endptr;
    SF_C_TYPE ctype = m_metadata[m_currColumnIdx].c_type;
    switch (ctype)
    {
        case SF_C_TYPE_BOOLEAN:
        {
            *out_data = strcmp("1", rawData->valuestring) == 0 ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            return SF_STATUS_SUCCESS;
        }
        case SF_C_TYPE_FLOAT64:
        {
            float64 floatVal = strtod(rawData->valuestring, &endptr);

            if (endptr == rawData->valuestring)
            {
                CXX_LOG_ERROR("Value cannot be converted from float64 to boolean.");
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            }
            if (floatVal == INFINITY || floatVal == -INFINITY)
            {
                CXX_LOG_ERROR("Value out of range for float64. Cannot convert to boolean.");
                return SF_STATUS_ERROR_OUT_OF_RANGE;
            }

            *out_data = floatVal == 0.0 ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case SF_C_TYPE_INT64:
        {
            int64 intVal = strtoll(rawData->valuestring, &endptr, 10);

            if (endptr == rawData->valuestring)
            {
                CXX_LOG_ERROR("Value cannot be converted from int64 to boolean.");
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            }

            *out_data = intVal == 0 ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case SF_C_TYPE_STRING:
        {
            *out_data = strlen(rawData->valuestring) == 0 ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        default:
            CXX_LOG_ERROR("Conversion to boolean unsupported for C type: %d", ctype);
            out_data = nullptr;
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsInt8(int8 * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    *out_data = static_cast<int8>(rawData->valuestring[0]);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsInt32(int32 * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    int64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtoll(rawData->valuestring, &endptr, 10);

    if (endptr == rawData->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to int32.");
        out_data = nullptr;
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (((value == LONG_MIN || value == LONG_MAX) && errno == ERANGE)
        || (value < SF_INT32_MIN || value > SF_INT32_MAX))
    {
        CXX_LOG_ERROR("Value out of range for int32.");
        out_data = nullptr;
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = static_cast<int32>(value);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsInt64(int64 * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    int64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtoll(rawData->valuestring, &endptr, 10);

    if (endptr == rawData->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to int64.");
        out_data = nullptr;
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if ((value == SF_INT64_MIN || value == SF_INT64_MAX) && errno == ERANGE)
    {
        CXX_LOG_ERROR("Value out of range for int64.");
        out_data = nullptr;
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = static_cast<int64>(value);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsUint8(uint8 * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    *out_data = static_cast<uint8>(rawData->valuestring[0]);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsUint32(uint32 * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    uint64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtoull(rawData->valuestring, &endptr, 10);

    if (endptr == rawData->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to uint32.");
        *out_data = 0;
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    bool isNeg = std::strchr(rawData->valuestring, '-') != nullptr ? true : false;

    if (((value == 0 || value == ULONG_MAX) && errno == ERANGE)
        || (isNeg && value < (SF_UINT64_MAX - SF_UINT32_MAX))
        || (!isNeg && value > SF_UINT32_MAX))
    {
        CXX_LOG_ERROR("Value out of range for uint32.");
        out_data = nullptr;
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    // If the input was negative, do a little trickery to get it into uint32.
    if (isNeg && value > SF_UINT32_MAX)
        value = value - (SF_UINT64_MAX - SF_UINT32_MAX);

    *out_data = static_cast<uint32>(value);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsUint64(uint64 * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    uint64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtoull(rawData->valuestring, &endptr, 10);

    if (endptr == rawData->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to uint64.");
        out_data = nullptr;
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if ((value == 0 || value == SF_UINT64_MAX) && errno == ERANGE)
    {
        CXX_LOG_ERROR("Value out of range for uint64.");
        out_data = nullptr;
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = value;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsFloat32(float32 * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    float32 value = 0.0;
    char * endptr;
    errno = 0;
    value = std::strtof(rawData->valuestring, &endptr);

    if (endptr == rawData->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to float32.");
        out_data = nullptr;
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (errno == ERANGE || value == INFINITY || value == -INFINITY)
    {
        CXX_LOG_ERROR("Value out of range for float32.");
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = value;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsFloat64(float64 * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    float64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtod(rawData->valuestring, &endptr);

    if (endptr == rawData->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to float64.");
        out_data = nullptr;
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (errno == ERANGE || value == -INFINITY || value == INFINITY)
    {
        CXX_LOG_ERROR("Value out of range for float64.");
        out_data = nullptr;
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = value;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsConstString(const char ** out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        *out_data = "";
        return SF_STATUS_SUCCESS;
    }

    *out_data = rawData->valuestring;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ResultSetJson::getCurrCellAsString(char ** out_data, size_t * io_len, size_t * io_capacity)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG(
            "Cell at row %d, column %d is null. Writing as empty string.",
            m_currRowIdx, m_currColumnIdx);

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

    char * strValue = snowflake_cJSON_GetStringValue(rawData);
    switch (m_metadata[m_currColumnIdx].type)
    {
        case SF_DB_TYPE_BOOLEAN:
            return convertBoolToString(strValue, out_data, io_len, io_capacity);
        case SF_DB_TYPE_DATE:
            return convertDateToString(strValue, out_data, io_len, io_capacity);
        case SF_DB_TYPE_TIME:
        case SF_DB_TYPE_TIMESTAMP_LTZ:
        case SF_DB_TYPE_TIMESTAMP_NTZ:
        case SF_DB_TYPE_TIMESTAMP_TZ:
            return convertTimeToString(strValue, out_data, io_len, io_capacity);
        default:
            std::strcpy(*out_data, strValue);
            return SF_STATUS_SUCCESS;
    }
}

SF_STATUS STDCALL ResultSetJson::getCurrCellAsTimestamp(SF_TIMESTAMP * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);
    SF_DB_TYPE db_type = m_metadata[m_currColumnIdx].type;

    if (snowflake_cJSON_IsNull(rawData))
    {
        CXX_LOG_DEBUG("Cell at row %d, column %d is null.", m_currRowIdx, m_currColumnIdx);
        return snowflake_timestamp_from_parts(out_data, 0, 0, 0, 0, 1, 1, 1970, 0, 9, db_type);
    }

    if (db_type == SF_DB_TYPE_DATE
        || db_type == SF_DB_TYPE_TIME
        || db_type == SF_DB_TYPE_TIMESTAMP_LTZ
        || db_type == SF_DB_TYPE_TIMESTAMP_NTZ
        || db_type == SF_DB_TYPE_TIMESTAMP_TZ)
    {
        return snowflake_timestamp_from_epoch_seconds(
            out_data,
            rawData->valuestring,
            m_tzString.c_str(),
            m_metadata[m_currColumnIdx].scale,
            db_type);
    }
    else
    {
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL ResultSetJson::getCurrCellStrlen(size_t * out_data)
{
    char * strValue = nullptr;
    size_t len = 0;
    size_t capacity = 0;

    SF_STATUS status = this->getCurrCellAsString(&strValue, &len, &capacity);
    if (status != SF_STATUS_SUCCESS)
        return status;

    *out_data = std::strlen(strValue);
    return SF_STATUS_SUCCESS;
}

cJSON * ResultSetJson::getCurrRow()
{
    return snowflake_cJSON_GetArrayItem(m_records, m_currRowIdx);
}

size_t ResultSetJson::getRowCountInChunk()
{
    return m_rowCountInChunk;
}

SF_STATUS STDCALL ResultSetJson::isCurrCellNull(sf_bool * out_data)
{
    cJSON * rawData = this->getCellValue(m_currRowIdx, m_currColumnIdx);
    *out_data = snowflake_cJSON_IsNull(rawData) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;

    return SF_STATUS_SUCCESS;
}

// Private methods =================================================================================

cJSON * ResultSetJson::getCellValue(uint32 rowIdx, uint32 colIdx)
{
    CXX_LOG_DEBUG("getCellValue -- Retrieving cell at row %d, column %d.", rowIdx, colIdx);
    cJSON * rowData = snowflake_cJSON_GetArrayItem(m_records, rowIdx);
    return snowflake_cJSON_GetArrayItem(rowData, colIdx);
}

} // namespace Client
} // namespace Snowflake
