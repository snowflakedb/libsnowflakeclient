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
        cJSON * rowset,
        SF_COLUMN_DESC * metadata,
        const char * tz_string
    )
    {
        rs_json_t * rs_struct = (rs_json_t *) SF_MALLOC(sizeof(rs_json_t));
        Snowflake::Client::ResultSetJson * rs_obj = new Snowflake::Client::ResultSetJson(
            rowset, metadata, std::string(tz_string));
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

    SF_STATUS STDCALL rs_json_get_cell_as_bool(rs_json_t * rs, size_t idx, sf_bool * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsBool(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_int8(rs_json_t * rs, size_t idx, int8 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsInt8(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_int32(rs_json_t * rs, size_t idx, int32 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsInt32(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_int64(rs_json_t * rs, size_t idx, int64 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsInt64(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_uint8(rs_json_t * rs, size_t idx, uint8 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsUint8(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_uint32(rs_json_t * rs, size_t idx, uint32 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsUint32(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_uint64(rs_json_t * rs, size_t idx, uint64 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsUint64(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_float32(rs_json_t * rs, size_t idx, float32 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsFloat32(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_float64(rs_json_t * rs, size_t idx, float64 * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsFloat64(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_const_string(
        rs_json_t * rs,
        size_t idx,
        const char ** out_data
    )
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsConstString(idx, out_data);
    }

    SF_STATUS STDCALL rs_json_get_cell_as_timestamp(
        rs_json_t * rs,
        size_t idx,
        SF_TIMESTAMP * out_data
    )
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellAsTimestamp(idx, out_data);
    }


    SF_STATUS STDCALL rs_json_get_cell_strlen(rs_json_t * rs, size_t idx, size_t * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->getCellStrlen(idx, out_data);
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

    SF_STATUS STDCALL rs_json_is_cell_null(rs_json_t * rs, size_t idx, sf_bool * out_data)
    {
        Snowflake::Client::ResultSetJson * rs_obj;

        if (rs == NULL)
        {
            return SF_STATUS_ERROR_NULL_POINTER;
        }

        rs_obj = static_cast<Snowflake::Client::ResultSetJson*> (rs->rs_object);
        return rs_obj->isCellNull(idx, out_data);
    }

#ifdef __cplusplus
} // extern "C"
#endif
