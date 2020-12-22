/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>

#include "memory.h"
#include "result_set_json.h"


result_set_json_t * result_set_json_create(
    cJSON * data,
    SF_CHUNK_DOWNLOADER * chunk_downloader,
    char * query_id,
    int32 tz_offset,
    int64 total_chunk_count,
    int64 total_column_count,
    int64 total_row_count
)
{
    result_set_json_t * rs_struct;
    rs_struct = (typeof(rs_struct)) SF_MALLOC(sizeof(*rs_struct));
    Snowflake::Client::ResultSetJson * rs_obj(
        data,
        chunk_downloader,
        std::string(query_id),
        tz_offset,
        total_chunk_count,
        total_column_count,
        total_row_count
    );
    rs_struct->result_set_object = rs_obj;

    return rs_struct;
}

void result_set_json_destroy(result_set_json_t * rs)
{
    if (rs == NULL)
    {
        return;
    }

    delete static_cast<Snowflake::Client::ResultSetJson *>(rs->result_set_object);
    SF_FREE(rs);
}

bool result_set_json_next_row(result_set_json_t * rs)
{
    Snowflake::Client::ResultSetJson * rs_obj;

    if (rs == NULL)
    {
        return false;
    }

    rs_obj = static_cast<Snowflake::Client::ResultSetJson *> (rs->result_set_object);
    return rs_obj->next();
}

cJSON * result_set_json_get_current_row(result_set_json_t * rs)
{
    Snowflake::Client::ResultSetJson * rs_obj;

    if (rs == NULL)
    {
        return NULL;
    }

    rs_obj = static_cast<Snowflake::Client::ResultSetJson *> (rs->result_set_object);
    return rs_obj->getCurrentRow();
}
