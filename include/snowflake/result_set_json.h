/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_RESULTSETJSON_H
#define SNOWFLAKE_RESULTSETJSON_H

#include "snowflake/basic_types.h"
#include "chunk_downloader.h"
#include "ResultSetJson.hpp"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * A result set interface for JSON result format.
     *
     * See:
     * - cpp/ResultSet.cpp
     * - cpp/ResultSetJson.cpp
     */
    struct result_set_json {
        void * result_set_object;
    };
    typedef struct result_set_json result_set_json_t;

    /**
     * Creates an empty result set.
     */
    result_set_json_t result_set_json_create_empty();

    /**
     * Parameterized constructor.
     * Initializes the result set with required information as well as data.
     *
     * @param data                      A pointer to the result set data.
     * @param chunk_downloader          A pointer to the chunk downloader object.
     * @param query_id                  The query ID as a C-style string.
     * @param tz_offset                 The time zone offset.
     * @param total_chunk_count         The total number of chunks.
     * @param total_column_count        The total number of columns.
     * @param total_row_count           The total number of rows.
     */
    result_set_json_t result_set_json_create(
        cJSON * data,
        SF_CHUNK_DOWNLOADER * chunk_downloader,
        char * query_id,
        int32 tz_offset,
        int64 total_chunk_count,
        int64 total_column_count,
        int64 total_row_count
    );

    /**
     * Destructor.
     */
    void result_set_json_destroy();

    /**
     * Advances to next row.
     * @return true if next row exists, false if next row does not exist.
     */
    bool result_set_json_next_row();

    /**
     * Get the data in the current row as a cJSON struct.
     *
     * @return The current row as a cJSON struct.
     */
    cJSON * result_set_json_get_current_row();



#ifdef __cplusplus
}
#endif

#endif // SNOWFLAKE_RESULTSET_H
