/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>

#include "result_set.h"
#include "SFLogger.h"


#ifdef __cplusplus
extern "C" {
#endif

    void * rs_create(
        cJSON * initial_chunk,
        SF_COLUMN_DESC * metadata,
        QUERY_RESULT_FORMAT query_result_format,
        char * timezone
    )
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_create(initial_chunk, metadata, std::string(timezone));
            case JSON_FORMAT:
                return rs_json_create(initial_chunk, metadata, std::string(timezone));
            default:
                CXX_LOG_ERROR("Unsupported query result format.");
                return nullptr;
        }
    }

    void rs_destroy(void * rs, QUERY_RESULT_FORMAT query_result_format)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                rs_arrow_destroy(rs);
                break;
            case JSON_FORMAT:
                rs_json_destroy(rs);
                break;
            default:
                CXX_LOG_ERROR("Unsupported query result format.");
                break;
        }
    }

    SF_STATUS STDCALL
    rs_append_chunk(void * rs, QUERY_RESULT_FORMAT query_result_format, cJSON * chunk)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_append_chunk(rs, chunk);
            case JSON_FORMAT:
                return rs_json_append_chunk(rs, chunk);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_finish_result_set(void * rs, QUERY_RESULT_FORMAT query_result_format)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_finish_result_set(rs);
            case JSON_FORMAT:
                return rs_json_finish_result_set(rs);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_next_column(void * rs, QUERY_RESULT_FORMAT query_result_format)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_next_column(rs);
            case JSON_FORMAT:
                return rs_json_next_column(rs);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_next_row(void * rs, QUERY_RESULT_FORMAT query_result_format)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_next_row(rs);
            case JSON_FORMAT:
                return rs_json_next_row(rs);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_bool(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        sf_bool * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_bool(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_bool(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_int8(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        int8 * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_int8(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_int8(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_int32(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        int32 * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_int32(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_int32(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_int64(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        int64 * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_int64(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_int64(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_uint8(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        uint8 * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_uint8(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_uint8(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_uint32(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        uint32 * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_uint32(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_uint32(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_uint64(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        uint64 * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_uint64(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_uint64(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_float64(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        float64 * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_float64(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_float64(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_const_string(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        const char ** out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_const_string(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_const_string(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_string(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        char * out_data,
        size_t * io_len,
        size_t * io_capacity)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_string(rs, out_data, io_len, io_capacity);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_string(rs, out_data, io_len, io_capacity);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    SF_STATUS STDCALL rs_get_curr_cell_as_timestamp(
        void * rs,
        QUERY_RESULT_FORMAT query_result_format,
        SF_TIMESTAMP * out_data)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_curr_cell_as_timestamp(rs, out_data);
            case JSON_FORMAT:
                return rs_json_get_curr_cell_as_timestamp(rs, out_data);
            default:
                return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
        }
    }

    cJSON * rs_get_curr_row(void * rs, QUERY_RESULT_FORMAT query_result_format)
    {
        switch (query_result_format)
        {
            case JSON_FORMAT:
                return rs_json_get_row_count_in_chunk(rs);
            case ARROW_FORMAT:
            default:
                return NULL;
        }
    }

    size_t rs_get_row_count_in_chunk(void * rs, QUERY_RESULT_FORMAT query_result_format)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_row_count_in_chunk(rs);
            case JSON_FORMAT:
                return rs_json_get_row_count_in_chunk(rs);
            default:
                return 0;
        }
    }

    size_t rs_get_total_row_count(void * rs, QUERY_RESULT_FORMAT query_result_format)
    {
        switch (query_result_format)
        {
            case ARROW_FORMAT:
                return rs_arrow_get_total_row_count(rs);
            case JSON_FORMAT:
                return rs_json_get_total_row_count(rs);
            default:
                return 0;
        }
    }

#ifdef __cplusplus
} // extern "C"
#endif
