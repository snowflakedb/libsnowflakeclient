/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include "result_set.h"


#ifdef __cplusplus
extern "C" {
#endif

    void * rs_create(
        cJSON * initial_chunk,
        SF_COLUMN_DESC * metadata,
        QueryResultFormat_t * query_result_format,
        const char * tz_string
    )
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_create(initial_chunk, metadata, tz_string);
            case JSON_FORMAT:
                return rs_json_create(initial_chunk, metadata, tz_string);
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
    rs_append_chunk(void * rs, QueryResultFormat_t * query_result_format, cJSON * chunk)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_append_chunk((rs_arrow_t *) rs, chunk);
            case JSON_FORMAT:
                return rs_json_append_chunk((rs_json_t *) rs, chunk);
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

    SF_STATUS STDCALL rs_next_column(void * rs, QueryResultFormat_t * query_result_format)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_next_column((rs_arrow_t *) rs);
            case JSON_FORMAT:
                return rs_json_next_column((rs_json_t *) rs);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_next_row(void * rs, QueryResultFormat_t * query_result_format)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_next_row((rs_arrow_t *) rs);
            case JSON_FORMAT:
                return rs_json_next_row((rs_json_t *) rs);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_bool(
        void * rs,
        QueryResultFormat_t * query_result_format,
        sf_bool * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_bool((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_bool((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_int8(
        void * rs,
        QueryResultFormat_t * query_result_format,
        int8 * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_int8((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_int8((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_int32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        int32 * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_int32((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_int32((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_int64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        int64 * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_int64((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_int64((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_uint8(
        void * rs,
        QueryResultFormat_t * query_result_format,
        uint8 * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_uint8((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_uint8((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_uint32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        uint32 * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_uint32((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_uint32((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_uint64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        uint64 * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_uint64((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_uint64((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_float32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        float32 * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_float32((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_float32((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_float64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        float64 * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_float64((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_float64((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_const_string(
        void * rs,
        QueryResultFormat_t * query_result_format,
        const char ** out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_const_string((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_const_string((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_string(
        void * rs,
        QueryResultFormat_t * query_result_format,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_string((rs_arrow_t *) rs, out_data, io_len, io_capacity);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_string((rs_json_t *) rs, out_data, io_len, io_capacity);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_timestamp(
        void * rs,
        QueryResultFormat_t * query_result_format,
        SF_TIMESTAMP * out_data)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_timestamp((rs_arrow_t *) rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_timestamp((rs_json_t *) rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    cJSON * rs_get_curr_row(void * rs, QueryResultFormat_t * query_result_format)
    {
        switch (*query_result_format)
        {
            case JSON_FORMAT:
                return rs_json_get_curr_row((rs_json_t *) rs);
            case ARROW_FORMAT:
            default:
                return NULL;
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

    size_t rs_get_total_row_count(void * rs, QueryResultFormat_t * query_result_format)
    {
        switch (*query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_total_row_count((rs_arrow_t *) rs);
            case JSON_FORMAT:
                return rs_json_get_total_row_count((rs_json_t *) rs);
            default:
                return 0;
        }
    }

#ifdef __cplusplus
} // extern "C"
#endif
