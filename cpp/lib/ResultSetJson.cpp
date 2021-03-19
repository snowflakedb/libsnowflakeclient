/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include <cerrno>
#include <cstring>
#include <stdlib.h>

#include "../logger/SFLogger.hpp"
#include "DataConversion.hpp"
#include "memory.h"
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
    cJSON * rowset,
    SF_COLUMN_DESC * metadata,
    std::string tzString
) :
    ResultSet(metadata, tzString)
{
    m_queryResultFormat = QueryResultFormat::JSON;
    m_totalColumnCount = snowflake_cJSON_GetArraySize(
        snowflake_cJSON_GetArrayItem(rowset, 0));
    m_chunk = nullptr;
    appendChunk(rowset);
}

ResultSetJson::~ResultSetJson()
{
    if (m_chunk != nullptr)
        snowflake_cJSON_Delete(m_chunk);
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
        return SF_STATUS_ERROR_BAD_JSON;
    }

    // Free previous chunk, if it exists, and set m_chunks to the given chunk.
    if (m_chunk != nullptr)
        snowflake_cJSON_Delete(m_chunk);
    m_chunk = chunk;

    // The initial state upon appending a new chunk.
    m_currChunkRowIdx = 0;
    m_currRow = nullptr;

    // Update other counts.
    m_rowCountInChunk = snowflake_cJSON_GetArraySize(m_chunk);
    m_totalChunkCount++;
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
    // If the current row is null, then this is the first call to next().
    // On the first call, we advance the iterator from a null state to the first element.
    if (m_currRow == nullptr)
    {
        m_currRow = m_chunk->child;
        return SF_STATUS_SUCCESS;
    }

    // If we've reached the end of the chunk, then maintain this state until reset by appendChunk().
    // Otherwise, traverse to the next row as usual.
    if (m_currRow->next == nullptr)
    {
        CXX_LOG_ERROR("next -- Already at end of current chunk.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }
    else
    {
        m_currRow = m_currRow->next;
        m_currChunkRowIdx++;
        m_currRowIdx++;
    }

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsBool(size_t idx, sf_bool * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = SF_BOOLEAN_FALSE;
        return SF_STATUS_SUCCESS;
    }

    char * endptr;
    SF_C_TYPE ctype = m_metadata[m_currColumnIdx].c_type;
    switch (ctype)
    {
        case SF_C_TYPE_BOOLEAN:
        {
            *out_data = strcmp("1", cell->valuestring) == 0 ?
                SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            return SF_STATUS_SUCCESS;
        }
        case SF_C_TYPE_FLOAT64:
        {
            float64 floatVal = strtod(cell->valuestring, &endptr);

            if (endptr == cell->valuestring)
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
            int64 intVal = strtoll(cell->valuestring, &endptr, 10);

            if (endptr == cell->valuestring)
            {
                CXX_LOG_ERROR("Value cannot be converted from int64 to boolean.");
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            }

            *out_data = intVal == 0 ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        case SF_C_TYPE_STRING:
        {
            *out_data = strlen(cell->valuestring) == 0 ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            return SF_STATUS_SUCCESS;
        }
        default:
            CXX_LOG_ERROR("Conversion to boolean unsupported for C type: %d", ctype);
            out_data = nullptr;
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL ResultSetJson::getCellAsInt8(size_t idx, int8 * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    *out_data = static_cast<int8>(cell->valuestring[0]);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsInt32(size_t idx, int32 * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    int64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtoll(cell->valuestring, &endptr, 10);

    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0)
        || endptr == cell->valuestring)
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

SF_STATUS STDCALL ResultSetJson::getCellAsInt64(size_t idx, int64 * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    int64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtoll(cell->valuestring, &endptr, 10);

    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0)
        || endptr == cell->valuestring)
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

    *out_data = value;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsUint8(size_t idx, uint8 * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    *out_data = static_cast<uint8>(cell->valuestring[0]);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsUint32(size_t idx, uint32 * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    uint64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtoull(cell->valuestring, &endptr, 10);

    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0) 
        || endptr == cell->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to uint32.");
        *out_data = 0;
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    bool isNeg = (std::strchr(cell->valuestring, '-') != NULL) ? true : false;

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

SF_STATUS STDCALL ResultSetJson::getCellAsUint64(size_t idx, uint64 * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    uint64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtoull(cell->valuestring, &endptr, 10);

    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0)
        || endptr == cell->valuestring)
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

SF_STATUS STDCALL ResultSetJson::getCellAsFloat32(size_t idx, float32 * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    float32 value = 0.0;
    char * endptr;
    errno = 0;
    value = std::strtof(cell->valuestring, &endptr);

    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0)
        || endptr == cell->valuestring)
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

SF_STATUS STDCALL ResultSetJson::getCellAsFloat64(size_t idx, float64 * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        out_data = nullptr;
        return SF_STATUS_SUCCESS;
    }

    float64 value = 0;
    char * endptr;
    errno = 0;
    value = std::strtod(cell->valuestring, &endptr);

    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0)
        || endptr == cell->valuestring)
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

SF_STATUS STDCALL ResultSetJson::getCellAsConstString(size_t idx, const char ** out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = NULL;
        return SF_STATUS_SUCCESS;
    }

    *out_data = cell->valuestring;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
ResultSetJson::getCellAsString(size_t idx, char ** out_data, size_t * io_len, size_t * io_capacity)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
        // Trivial allocation if necessary.
        if (io_capacity == nullptr)
            io_capacity = new size_t;
        *io_capacity = 1;

        if (*out_data != nullptr)
            delete *out_data;
        *out_data = new char[*io_capacity];
        (*out_data)[0] = '\0';

        return SF_STATUS_SUCCESS;
    }

    char * strValue = cell->valuestring;
    int64 scale = m_metadata[m_currColumnIdx].scale;
    SF_DB_TYPE snowType = m_metadata[m_currColumnIdx].type;

    switch (snowType)
    {
        case SF_DB_TYPE_BOOLEAN:
            return Conversion::Json::BoolToString(strValue, out_data, io_len, io_capacity);
        case SF_DB_TYPE_DATE:
            return Conversion::Json::DateToString(strValue, out_data, io_len, io_capacity);
        case SF_DB_TYPE_TIME:
        case SF_DB_TYPE_TIMESTAMP_LTZ:
        case SF_DB_TYPE_TIMESTAMP_NTZ:
        case SF_DB_TYPE_TIMESTAMP_TZ:
            return Conversion::Json::TimeToString(
                strValue, scale, snowType, m_tzString,
                out_data, io_len, io_capacity);
        default:
            size_t len = std::strlen(strValue);
            Snowflake::Client::Util::AllocateCharBuffer(out_data, io_len, io_capacity, len);
            std::strncpy(*out_data, strValue, len);
            (*out_data)[len] = '\0';
            return SF_STATUS_SUCCESS;
    }
}

SF_STATUS STDCALL ResultSetJson::getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;
    SF_DB_TYPE snowType = m_metadata[m_currColumnIdx].type;

    if (snowflake_cJSON_IsNull(cell))
    {
        return snowflake_timestamp_from_parts(out_data, 0, 0, 0, 0, 1, 1, 1970, 0, 9, SF_DB_TYPE_TIMESTAMP_NTZ);
    }

    if (snowType == SF_DB_TYPE_DATE
        || snowType == SF_DB_TYPE_TIME
        || snowType == SF_DB_TYPE_TIMESTAMP_LTZ
        || snowType == SF_DB_TYPE_TIMESTAMP_NTZ
        || snowType == SF_DB_TYPE_TIMESTAMP_TZ)
    {
        return snowflake_timestamp_from_epoch_seconds(
            out_data,
            cell->valuestring,
            m_tzString.c_str(),
            m_metadata[m_currColumnIdx].scale,
            snowType);
    }
    else
    {
        CXX_LOG_ERROR("Not a valid type for Timestamp conversion: %d.", snowType);
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL ResultSetJson::getCellStrlen(size_t idx, size_t * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    if (snowflake_cJSON_IsNull(cell))
    {
      *out_data = 0;
    }
    else
    {
      *out_data = std::strlen(cell->valuestring);
    }

    return SF_STATUS_SUCCESS;
}

size_t ResultSetJson::getRowCountInChunk()
{
    return m_rowCountInChunk;
}

SF_STATUS STDCALL ResultSetJson::isCellNull(size_t idx, sf_bool * out_data)
{
    if (m_currRow == nullptr && m_currRowIdx == m_totalRowCount)
    {
        CXX_LOG_ERROR("Trying to retrieve cell when already advanced past end of result set.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    if (idx < 1 || idx > m_totalColumnCount)
    {
        CXX_LOG_ERROR("Trying to retrieve out-of-bounds cell index.");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    *out_data = snowflake_cJSON_IsNull(cell) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;

    return SF_STATUS_SUCCESS;
}

} // namespace Client
} // namespace Snowflake
