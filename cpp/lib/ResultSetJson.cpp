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
    ResultSet(SF_JSON_FORMAT)
{
    ; // Do nothing
}

ResultSetJson::ResultSetJson(
    cJSON * rowset,
    SF_COLUMN_DESC * metadata,
    const std::string& tzString
) :
    ResultSet(metadata, tzString, SF_JSON_FORMAT)
{
    m_chunk = nullptr;
    appendChunk(rowset);
}

ResultSetJson::~ResultSetJson()
{
    if (m_chunk != nullptr)
        snowflake_cJSON_Delete(m_chunk);
}

// Public methods ==================================================================================

SF_STATUS STDCALL ResultSetJson::appendChunk(void* chunkPtr)
{
    cJSON * chunk = (cJSON *)chunkPtr;
    if (chunk == nullptr)
    {
        CXX_LOG_ERROR("appendChunk -- Received a null chunk to append.");
        setError(SF_STATUS_ERROR_NULL_POINTER, "Received a null chunk to append.");
        return SF_STATUS_ERROR_NULL_POINTER;
    }

    if (!snowflake_cJSON_IsArray(chunk))
    {
        CXX_LOG_ERROR("appendChunk -- Given chunk is not of type array.");
        setError(SF_STATUS_ERROR_BAD_JSON, "Given chunk is not of type array.");
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
    if (m_isFirstChunk)
    {
        m_totalColumnCount = snowflake_cJSON_GetArraySize(m_chunk->child);
        if (0 == m_totalColumnCount)
        {
            m_rowCountInChunk = 0;
            return SF_STATUS_SUCCESS;
        }
        m_isFirstChunk = false;
    }
    m_rowCountInChunk = snowflake_cJSON_GetArraySize(m_chunk);
    m_totalChunkCount++;
    CXX_LOG_DEBUG("appendChunk -- Appended chunk of size %d.", m_rowCountInChunk);

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
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    // Set default value for error or null cases.
    *out_data = SF_BOOLEAN_FALSE;

    if (snowflake_cJSON_IsNull(cell))
    {
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
                setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                    "Value cannot be converted from float64 to boolean.");
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            }
            if (floatVal == INFINITY || floatVal == -INFINITY)
            {
                CXX_LOG_ERROR("Value out of range for float64. Cannot convert to boolean.");
                setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                    "Value out of range for float64. Cannot convert to boolean.");
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
                setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                    "Value cannot be converted from int64 to boolean.");
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
            setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
                "No valid conversion to boolean from data type.");
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL ResultSetJson::getCellAsInt8(size_t idx, int8 * out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
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

SF_STATUS STDCALL ResultSetJson::getCellAsInt32(size_t idx, int32* out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON* cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    // Set default value for error or null cases.
    *out_data = 0;

    if (snowflake_cJSON_IsNull(cell))
    {
        return SF_STATUS_SUCCESS;
    }

    int64 value = 0;
    char* endptr;
    errno = 0;
    if (std::strchr(cell->valuestring, 'e') != NULL) {
        //Scientific notation minimum scale is 38 in the snowflake db, which menas it is out of int64 range or too small like 1e38 or 1e-38
        float64 v = std::strtod(cell->valuestring, &endptr);
        if (v < 1 && v > -1)
        {
            // The conversion error will be occurred in below check
            value = 0;
        }
        else
        {
            CXX_LOG_ERROR("Cannot convert value to uint32. Scientific notion value is too big or too small to convert to uint32");
            setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Cannot convert value to uint32.");
            return SF_STATUS_ERROR_OUT_OF_RANGE;
        }
    }
    else
    {
        value = std::strtoll(cell->valuestring, &endptr, 10);
    }

    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0)
        || endptr == cell->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to int32.");
        setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "Cannot convert value to int32.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (((value == LONG_MIN || value == LONG_MAX) && errno == ERANGE)
        || (value < SF_INT32_MIN || value > SF_INT32_MAX))
    {
        CXX_LOG_ERROR("Value out of range for int32.");
        setError(SF_STATUS_ERROR_OUT_OF_RANGE,
            "Value out of range for int32.");
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = static_cast<int32>(value);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsInt64(size_t idx, int64 * out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    // Set default value for error or null cases.
    *out_data = 0;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    int64 value = 0;
    char * endptr;
    errno = 0;
    if (std::strchr(cell->valuestring, 'e') != NULL) {
        //Scientific notation minimum scale is 38 in the snowflake db, which menas it is out of int64 range or too small like 1e38 or 1e-38
        float64 v = std::strtod(cell->valuestring, &endptr);
        if (v < 1 && v > -1)
        {
            // The conversion error will be occurred in below check
            value = 0;
        }
        else
        {
            CXX_LOG_ERROR("Cannot convert value to uint32. Scientific notion value is too big or too small to convert to uint32");
            setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Cannot convert value to uint32.");
            return SF_STATUS_ERROR_OUT_OF_RANGE;
        }
    }
    else
    {
        value = std::strtoll(cell->valuestring, &endptr, 10);
    }

    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0)
        || endptr == cell->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to int64.");
        setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "Cannot convert value to int64.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }


    if ((value == SF_INT64_MIN || value == SF_INT64_MAX) && errno == ERANGE)
    {
        CXX_LOG_ERROR("Value out of range for int64.");
        setError(SF_STATUS_ERROR_OUT_OF_RANGE,
            "Value out of range for int64.");
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = value;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsUint8(size_t idx, uint8 * out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
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
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    // Set default value for error or null cases.
    *out_data = 0;

    if (snowflake_cJSON_IsNull(cell))
    {
        *out_data = 0;
        return SF_STATUS_SUCCESS;
    }

    uint64 value = 0;
    char* endptr = NULL;
    errno = 0;
    if (std::strchr(cell->valuestring, 'e') != NULL) {
        //Scientific notation minimum scale is 38 in the snowflake db, which menas it is out of int64 range or too small like 1e38 or 1e-38
        float64 v = std::strtod(cell->valuestring, &endptr);
        if (v < 1 && v > -1)
        {
            // The conversion error will be occurred in below check
            value = 0;
        }
        else
        {
            CXX_LOG_ERROR("Cannot convert value to uint32. Scientific notion value is too big or too small to convert to uint32");
            setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Cannot convert value to uint32.");
            return SF_STATUS_ERROR_OUT_OF_RANGE;
        }
    }
    else
    {
        value = std::strtoull(cell->valuestring, &endptr, 10);
    }


    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0) 
        || endptr == cell->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to uint32.");
        setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "Cannot convert value to uint32.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    bool isNeg = (std::strchr(cell->valuestring, '-') != NULL) ? true : false;

    if (((value == 0 || value == ULONG_MAX) && errno == ERANGE)
        || (isNeg && value < (SF_UINT64_MAX - SF_UINT32_MAX))
        || (!isNeg && value > SF_UINT32_MAX))
    {
        CXX_LOG_ERROR("Value out of range for uint32.");
        setError(SF_STATUS_ERROR_OUT_OF_RANGE,
            "Value out of range for uint32.");
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
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    // Set default value for error or null cases.
    *out_data = 0;

    if (snowflake_cJSON_IsNull(cell))
    {
        return SF_STATUS_SUCCESS;
    }

    uint64 value = 0;
    char * endptr;
    errno = 0;
    if (std::strchr(cell->valuestring, 'e') != NULL) {
        //Scientific notation minimum scale is 38 in the snowflake db, which menas it is out of int64 range or too small like 1e38 or 1e-38
        float64 v = std::strtod(cell->valuestring, &endptr);
        if (v < 1 && v > -1) 
        {
            // The conversion error will be occurred in below check
            value = 0;
        }
        else 
        {
            CXX_LOG_ERROR("Cannot convert value to uint32. Scientific notion value is too big or too small to convert to uint32");
            setError(SF_STATUS_ERROR_OUT_OF_RANGE,
                "Cannot convert value to uint32.");
            return SF_STATUS_ERROR_OUT_OF_RANGE;
        }
    }
    else 
    {
        value = std::strtoull(cell->valuestring, &endptr, 10);
    }


    if ((value == 0 && std::strcmp(cell->valuestring, "0") != 0)
        || endptr == cell->valuestring)
    {
        CXX_LOG_ERROR("Cannot convert value to uint64.");
        setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "Cannot convert value to uint64.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if ((value == 0 || value == SF_UINT64_MAX) && errno == ERANGE)
    {
        CXX_LOG_ERROR("Value out of range for uint64.");
        setError(SF_STATUS_ERROR_OUT_OF_RANGE,
            "Value out of range for uint64.");
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = value;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsFloat32(size_t idx, float32 * out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    // Set default value for error or null cases.
    *out_data = 0.0;

    if (snowflake_cJSON_IsNull(cell))
    {
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
        setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "Cannot convert value to float32.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (errno == ERANGE || value == INFINITY || value == -INFINITY)
    {
        CXX_LOG_ERROR("Value out of range for float32.");
        setError(SF_STATUS_ERROR_OUT_OF_RANGE,
            "Value out of range for float32.");
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = value;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsFloat64(size_t idx, float64 * out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    // Set default value for error or null cases.
    *out_data = 0.0;

    if (snowflake_cJSON_IsNull(cell))
    {
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
        setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "Cannot convert value to float64.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (errno == ERANGE || value == -INFINITY || value == INFINITY)
    {
        CXX_LOG_ERROR("Value out of range for float64.");
        setError(SF_STATUS_ERROR_OUT_OF_RANGE,
            "Value out of range for float64.");
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = value;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetJson::getCellAsConstString(size_t idx, const char ** out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
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

SF_STATUS STDCALL ResultSetJson::getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;
    SF_DB_TYPE snowType = m_metadata[m_currColumnIdx].type;
    SF_STATUS status = SF_STATUS_SUCCESS;

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
        status = snowflake_timestamp_from_epoch_seconds(
            out_data,
            cell->valuestring,
            m_tzString.c_str(),
            m_metadata[m_currColumnIdx].scale,
            snowType);

        if (SF_STATUS_SUCCESS != status)
        {
            setError(status, "Failed to convert value to timestamp.");
        }
        return status;
    }
    else
    {
        CXX_LOG_ERROR("Not a valid type for Timestamp conversion: %d.", snowType);
        setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
            "Not a valid type for Timestamp conversion.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL ResultSetJson::getCellStrlen(size_t idx, size_t * out_data)
{
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
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
    if (idx < 1 || idx > m_totalColumnCount)
    {
        setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,
            "Column index must be between 1 and snowflake_num_fields()");
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON * cell = snowflake_cJSON_GetArrayItem(m_currRow, idx - 1);
    m_currColumnIdx = idx - 1;

    *out_data = snowflake_cJSON_IsNull(cell) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;

    return SF_STATUS_SUCCESS;
}

} // namespace Client
} // namespace Snowflake
