/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>

#include "memory.h"
#include "result_set_json.h"
#include "ResultSetJson.hpp"


#ifdef __cplusplus
extern "C" {
#endif

    rs_json_t * rs_json_create(
        cJSON * initial_chunk,
        SF_COLUMN_DESC * metadata,
        const char * tz_string
    )
    {
        rs_json_t * rs_struct = (rs_json_t *) SF_MALLOC(sizeof(rs_json_t));
        Snowflake::Client::ResultSetJson * rs_obj =
            new Snowflake::Client::ResultSetJson(initial_chunk, metadata, std::string(tz_string));
        rs_struct->rs_object = rs_obj;

        return rs_struct;
    }

    void rs_json_destroy(rs_json_t * rs)
    {
        if (rs == NULL)
        {
            return;
        }

        delete static_cast<Snowflake::Client::ResultSetJson*>(rs->rs_object);
        SF_FREE(rs);
    }

    SF_STATUS STDCALL rs_json_append_chunk(rs_json_t * rs, cJSON * chunk)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->appendChunk(chunk);
    }

    SF_STATUS STDCALL rs_json_finish_result_set(rs_json_t * rs)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->finishResultSet();
    }

    SF_STATUS STDCALL rs_json_next(rs_json_t * rs)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->next();
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_bool(rs_json_t * rs, sf_bool * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsBool(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_int8(rs_json_t * rs, int8 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsInt8(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_int32(rs_json_t * rs, int32 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsInt32(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_int64(rs_json_t * rs, int64 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsInt64(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_uint8(rs_json_t * rs, uint8 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsUint8(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_uint32(rs_json_t * rs, uint32 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsUint32(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_uint64(rs_json_t * rs, uint64 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsUint64(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_float32(rs_json_t * rs, float32 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsFloat32(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_float64(rs_json_t * rs, float64 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsFloat64(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_const_string(
        rs_json_t * rs,
        const char ** out_data
    )
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsConstString(out_data);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_string(
        rs_json_t * rs,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity
    )
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsString(out_data, io_len, io_capacity);
    }

    SF_STATUS STDCALL rs_json_get_curr_cell_as_timestamp(
        rs_json_t * rs,
        SF_TIMESTAMP * out_data
    )
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellAsTimestamp(out_data);
    }


    SF_STATUS STDCALL rs_json_get_curr_cell_strlen(rs_json_t * rs, size_t * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrCellStrlen(out_data);
    }

    cJSON * rs_json_get_curr_row(rs_json_t * rs)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return NULL;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCurrRow();
    }

    size_t rs_json_get_row_count_in_chunk(rs_json_t * rs)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return 0;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getRowCountInChunk();
    }

    size_t rs_json_get_total_row_count(rs_json_t * rs)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return 0;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getTotalRowCount();
    }

    SF_STATUS STDCALL rs_json_is_curr_cell_null(rs_json_t * rs, sf_bool * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->isCurrCellNull(out_data);
    }

#ifdef __cplusplus
} // extern "C"
#endif
