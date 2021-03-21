/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include "arrowheaders.hpp"

#include <string>

#include "memory.h"
#include "result_set_arrow.h"
#include "ResultSetArrow.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SF_WIN32
    rs_arrow_t * rs_arrow_create_with_json_result(
        cJSON * json_rowset64,
        SF_COLUMN_DESC * metadata,
        const char * tz_string
    )
    {
        arrow::BufferBuilder* bufferBuilder = NULL;
        if (json_rowset64)
        {
            const char * base64RowsetStr = snowflake_cJSON_GetStringValue(json_rowset64);
            if (base64RowsetStr && strlen(base64RowsetStr) > 0)
            {
                // Decode Base64-encoded Arrow-format rowset of the chunk and build a buffer builder from it.
                std::string decodedRowsetStr = arrow::util::base64_decode(std::string(base64RowsetStr));
                bufferBuilder = new arrow::BufferBuilder();
                bufferBuilder->Append((void *)decodedRowsetStr.c_str(), decodedRowsetStr.length());
            }
        }

        rs_arrow_t * rs_struct = (rs_arrow_t *) SF_MALLOC(sizeof(rs_arrow_t));
        Snowflake::Client::ResultSetArrow * rs_obj =
            new Snowflake::Client::ResultSetArrow(bufferBuilder, metadata, std::string(tz_string));
        rs_struct->rs_object = rs_obj;

        return rs_struct;
    }

    rs_arrow_t * rs_arrow_create_with_chunk(
      NON_JSON_RESP * initial_chunk,
      SF_COLUMN_DESC * metadata,
      const char * tz_string
    )
    {
      rs_arrow_t * rs_struct = (rs_arrow_t *)SF_MALLOC(sizeof(rs_arrow_t));
      Snowflake::Client::ResultSetArrow * rs_obj =
        new Snowflake::Client::ResultSetArrow((arrow::BufferBuilder*)(initial_chunk->buffer), metadata, std::string(tz_string));
      rs_struct->rs_object = rs_obj;

      // the buffer is passed in to result set so the response is no longer needed
      delete initial_chunk;

      return rs_struct;
    }

    void rs_arrow_destroy(rs_arrow_t * rs)
    {
        if (rs == NULL)
        {
            return;
        }

        delete static_cast<Snowflake::Client::ResultSetArrow*>(rs->rs_object);
        SF_FREE(rs);
    }

    SF_STATUS STDCALL rs_arrow_append_chunk(rs_arrow_t * rs, NON_JSON_RESP * chunk)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);

        arrow::BufferBuilder * buffer = (arrow::BufferBuilder*)(chunk->buffer);
        delete chunk;
        return rs_obj->appendChunk(buffer);
    }

    SF_STATUS STDCALL rs_arrow_next(rs_arrow_t * rs)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->next();
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_bool(rs_arrow_t * rs, size_t idx, sf_bool * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsBool(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_int8(rs_arrow_t * rs, size_t idx, int8 * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsInt8(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_int32(rs_arrow_t * rs, size_t idx, int32 * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsInt32(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_int64(rs_arrow_t * rs, size_t idx, int64 * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsInt64(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_uint8(rs_arrow_t * rs, size_t idx, uint8 * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsUint8(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_uint32(rs_arrow_t * rs, size_t idx, uint32 * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsUint32(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_uint64(rs_arrow_t * rs, size_t idx, uint64 * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsUint64(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_float32(rs_arrow_t * rs, size_t idx, float32 * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsFloat32(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_float64(rs_arrow_t * rs, size_t idx, float64 * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsFloat64(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_const_string(
        rs_arrow_t * rs,
        size_t idx,
        const char ** out_data
    )
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsConstString(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_string(
        rs_arrow_t * rs,
        size_t idx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity
    )
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsString(idx, out_data, io_len, io_capacity);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_as_timestamp(
        rs_arrow_t * rs,
        size_t idx,
        SF_TIMESTAMP * out_data
    )
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellAsTimestamp(idx, out_data);
    }

    SF_STATUS STDCALL rs_arrow_get_cell_strlen(rs_arrow_t * rs, size_t idx, size_t * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getCellStrlen(idx, out_data);
    }

    size_t rs_arrow_get_row_count_in_chunk(rs_arrow_t * rs)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return 0;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->getRowCountInChunk();
    }

    SF_STATUS STDCALL rs_arrow_is_cell_null(rs_arrow_t * rs, size_t idx, sf_bool * out_data)
    {
        Snowflake::Client::ResultSetArrow * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetArrow*> (rs->rs_object);
        return rs_obj->isCellNull(idx, out_data);
    }

    size_t arrow_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
    {
        size_t data_size = size * nmemb;
        arrow::BufferBuilder * arrowBufBuilder = (arrow::BufferBuilder*)(userdata);

        log_debug("Curl response for arrow chunk size: %zu", data_size);
        arrowBufBuilder->Append(ptr, data_size);
        return data_size;
    }

    NON_JSON_RESP* callback_create_arrow_resp(void)
    {
        NON_JSON_RESP* arrow_resp = new NON_JSON_RESP;
        arrow_resp->buffer = new arrow::BufferBuilder();
        arrow_resp->write_callback = arrow_write_callback;
        return arrow_resp;
    }

#else  //  SF_WIN32
rs_arrow_t * rs_arrow_create_with_json_result(
    cJSON * json_rowset64,
    SF_COLUMN_DESC * metadata,
    const char * tz_string
)
{
    log_error("Query results were fetched using Arrow");
    return NULL;
}

rs_arrow_t * rs_arrow_create_with_chunk(
    NON_JSON_RESP * initial_chunk,
    SF_COLUMN_DESC * metadata,
    const char * tz_string
)
{
    log_error("Query results were fetched using Arrow");
    return NULL;
}

void rs_arrow_destroy(rs_arrow_t * rs)
{
    log_error("Query results were fetched using Arrow");
}

SF_STATUS STDCALL rs_arrow_append_chunk(rs_arrow_t * rs, NON_JSON_RESP * chunk)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_finish_result_set(rs_arrow_t * rs)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_next(rs_arrow_t * rs)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_bool(rs_arrow_t * rs, size_t idx, sf_bool * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_int8(rs_arrow_t * rs, size_t idx, int8 * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_int32(rs_arrow_t * rs, size_t idx, int32 * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_int64(rs_arrow_t * rs, size_t idx, int64 * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_uint8(rs_arrow_t * rs, size_t idx, uint8 * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_uint32(rs_arrow_t * rs, size_t idx, uint32 * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_uint64(rs_arrow_t * rs, size_t idx, uint64 * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_float32(rs_arrow_t * rs, size_t idx, float32 * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_float64(rs_arrow_t * rs, size_t idx, float64 * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_const_string(
    rs_arrow_t * rs,
    size_t idx,
    const char ** out_data
)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_string(
    rs_arrow_t * rs,
    size_t idx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_as_timestamp(
    rs_arrow_t * rs,
    size_t idx,
    SF_TIMESTAMP * out_data
)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

SF_STATUS STDCALL rs_arrow_get_cell_strlen(rs_arrow_t * rs, size_t idx, size_t * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

size_t rs_arrow_get_row_count_in_chunk(rs_arrow_t * rs)
{
    log_error("Query results were fetched using Arrow");
    return 0;
}

size_t rs_arrow_get_total_row_count(rs_arrow_t * rs)
{
    log_error("Query results were fetched using Arrow");
    return 0;
}

SF_STATUS STDCALL rs_arrow_is_cell_null(rs_arrow_t * rs, size_t idx, sf_bool * out_data)
{
    log_error("Query results were fetched using Arrow");
    return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
}

size_t arrow_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    log_error("Query results were fetched using Arrow");
    return 0;
}

NON_JSON_RESP* callback_create_arrow_resp(void)
{
    log_error("Query results were fetched using Arrow");
    return NULL;
}

#endif //  SF_WIN32

#ifdef __cplusplus
} // extern "C"
#endif
