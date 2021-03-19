/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include "result_set.h"


#ifdef __cplusplus
extern "C" {
#endif

    void * rs_create_with_json_result(
        cJSON * json_rowset,
        SF_COLUMN_DESC * metadata,
        QueryResultFormat_t * query_result_format,
        const char * tz_string
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_create_with_json_result(json_rowset, metadata, tz_string);
            case JSON_FORMAT:
                return rs_json_create(json_rowset, metadata, tz_string);
            default:
                return nullptr;
        }
    }

    void * rs_create_with_chunk(
        void * initial_chunk,
        SF_COLUMN_DESC * metadata,
        QueryResultFormat_t * query_result_format,
        const char * tz_string
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_create_with_chunk((NON_JSON_RESP*)initial_chunk, metadata, tz_string);
            case JSON_FORMAT:
                return rs_json_create((cJSON*)initial_chunk, metadata, tz_string);
            default:
                return nullptr;
        }
    }

    void rs_destroy(void * rs, QueryResultFormat_t * query_result_format)
    {
        switch (*query_result_format){
            case ARROW_FORMAT:
                rs_arrow_destroy((rs_arrow_t *) rs);
                break;
            case JSON_FORMAT:
                rs_json_destroy((rs_json_t *) rs);
                break;
            default:
                break;
        }
    }

    SF_STATUS STDCALL
    rs_append_chunk(void * rs, QueryResultFormat_t * query_result_format, void * chunk)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_append_chunk((rs_arrow_t *) rs, (NON_JSON_RESP*)chunk);
            case JSON_FORMAT:
                return rs_json_append_chunk((rs_json_t *) rs, (cJSON*)chunk);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_finish_result_set(void * rs, QueryResultFormat_t * query_result_format)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_finish_result_set((rs_arrow_t *) rs);
            case JSON_FORMAT:
                return rs_json_finish_result_set((rs_json_t *) rs);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_next(void * rs, QueryResultFormat_t * query_result_format)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_next((rs_arrow_t *) rs);
            case JSON_FORMAT:
                return rs_json_next((rs_json_t *) rs);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_bool(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        sf_bool * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_bool((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_bool((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_int8(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        int8 * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_int8((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_int8((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_int32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        int32 * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_int32((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_int32((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_int64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        int64 * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_int64((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_int64((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_uint8(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        uint8 * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_uint8((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_uint8((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_uint32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        uint32 * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_uint32((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_uint32((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_uint64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        uint64 * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_uint64((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_uint64((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_float32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        float32 * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_float32((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_float32((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_float64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        float64 * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_float64((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_float64((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_const_string(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        const char ** out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_const_string((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_const_string((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_string(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_string(
                    (rs_arrow_t *) rs, idx, out_data, io_len, io_capacity);
            case JSON_FORMAT:
                return rs_json_get_cell_as_string(
                    (rs_json_t *) rs, idx, out_data, io_len, io_capacity);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_as_timestamp(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        SF_TIMESTAMP * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_as_timestamp((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_as_timestamp((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_cell_strlen(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        size_t * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_cell_strlen((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_get_cell_strlen((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    size_t rs_get_row_count_in_chunk(void * rs, QueryResultFormat_t * query_result_format)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_row_count_in_chunk((rs_arrow_t *) rs);
            case JSON_FORMAT:
                return rs_json_get_row_count_in_chunk((rs_json_t *) rs);
            default:
                return 0;
        }
    }

    SF_STATUS STDCALL rs_is_cell_null(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t idx,
        sf_bool * out_data
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_is_cell_null((rs_arrow_t *) rs, idx, out_data);
            case JSON_FORMAT:
                return rs_json_is_cell_null((rs_json_t *) rs, idx, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

#ifdef __cplusplus
} // extern "C"
#endif
