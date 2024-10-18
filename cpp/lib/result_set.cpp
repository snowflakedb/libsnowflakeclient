/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include "result_set.h"
#include "ResultSet.hpp"
#include "ResultSetArrow.hpp"
#include "ResultSetJson.hpp"

#ifdef __cplusplus
extern "C" {
#endif

    result_set_ptr rs_create_with_json_result(
        cJSON * json_rowset,
        SF_COLUMN_DESC * metadata,
        QueryResultFormat query_result_format,
        const char * tz_string
    )
    {
        return Snowflake::Client::ResultSet::CreateResultFromJson(
            json_rowset, metadata, query_result_format, std::string(tz_string));
    }

    result_set_ptr rs_create_with_chunk(
        void * initial_chunk,
        SF_COLUMN_DESC * metadata,
        QueryResultFormat query_result_format,
        const char * tz_string
    )
    {
        return Snowflake::Client::ResultSet::CreateResultFromChunk(
            initial_chunk, metadata, query_result_format, std::string(tz_string));
    }

    void rs_destroy(result_set_ptr rs)
    {
        if (!rs)
        {
            return;
        }
        delete static_cast<Snowflake::Client::ResultSet*>(rs);
    }

#define ERROR_IF_NULL(ptr)                                    \
{                                                             \
    if (ptr == NULL) { return SF_STATUS_ERROR_NULL_POINTER; } \
}

    SF_STATUS STDCALL
    rs_append_chunk(result_set_ptr rs, void * chunk)
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->appendChunk(chunk);
    }

    SF_STATUS STDCALL rs_next(result_set_ptr rs)
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->next();
    }

    SF_STATUS STDCALL rs_get_cell_as_bool(
        result_set_ptr rs,
        size_t idx,
        sf_bool * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsBool(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_int8(
        result_set_ptr rs,
        size_t idx,
        int8 * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsInt8(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_int32(
        result_set_ptr rs,
        size_t idx,
        int32 * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsInt32(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_int64(
        result_set_ptr rs,
        size_t idx,
        int64 * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsInt64(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_uint8(
        result_set_ptr rs,
        size_t idx,
        uint8 * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsUint8(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_uint32(
        result_set_ptr rs,
        size_t idx,
        uint32 * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsUint32(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_uint64(
        result_set_ptr rs,
        size_t idx,
        uint64 * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsUint64(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_float32(
        result_set_ptr rs,
        size_t idx,
        float32 * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsFloat32(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_float64(
        result_set_ptr rs,
        size_t idx,
        float64 * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsFloat64(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_const_string(
        result_set_ptr rs,
        size_t idx,
        const char ** out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsConstString(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_as_timestamp(
        result_set_ptr rs,
        size_t idx,
        SF_TIMESTAMP * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellAsTimestamp(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_cell_strlen(
        result_set_ptr rs,
        size_t idx,
        size_t * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getCellStrlen(idx, out_data);
    }

    size_t rs_get_row_count_in_chunk(result_set_ptr rs)
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getRowCountInChunk();
    }

    SF_STATUS STDCALL rs_is_cell_null(
        result_set_ptr rs,
        size_t idx,
        sf_bool * out_data
    )
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->isCellNull(idx, out_data);
    }

    SF_STATUS STDCALL rs_get_error(result_set_ptr rs)
    {
        ERROR_IF_NULL(rs);
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getError();
    }

    const char* rs_get_error_message(result_set_ptr rs)
    {
        if (!rs)
        {
            return "";
        }
        return static_cast<Snowflake::Client::ResultSet*>(rs)->getErrorMessage();
    }

#ifndef SF_WIN32
    size_t arrow_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
    {
        size_t data_size = size * nmemb;
        arrow::BufferBuilder * arrowBufBuilder = (arrow::BufferBuilder*)(userdata);

        log_debug("Curl response for arrow chunk size: %zu", data_size);
        (void) arrowBufBuilder->Append(ptr, data_size);
        return data_size;
    }

    NON_JSON_RESP* callback_create_arrow_resp(void)
    {
        NON_JSON_RESP* arrow_resp = new NON_JSON_RESP;
        arrow_resp->buffer = new arrow::BufferBuilder();
        arrow_resp->write_callback = arrow_write_callback;
        return arrow_resp;
    }
#else
    NON_JSON_RESP* callback_create_arrow_resp(void)
    {
        log_error("Query results were fetched using Arrow");
        return NULL;
    }
#endif

#ifdef __cplusplus
} // extern "C"
#endif
