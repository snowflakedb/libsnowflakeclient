/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_RESULTSETARROW_H
#define SNOWFLAKE_RESULTSETARROW_H

#include "snowflake/basic_types.h"
#include "chunk_downloader.h"
#include "ResultSetArrow.hpp"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * A result set interface for Arrow result format.
     *
     * @see cpp/ResultSet.cpp
     * @see cpp/ResultSetArrow.cpp
     */
    struct result_set_arrow {
        void * result_set_object;
    };
    typedef struct result_set_arrow result_set_arrow_t;

    /**
     * Creates an empty result set.
     */
    result_set_arrow_t * result_set_arrow_create_empty();

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
    result_set_arrow_t * result_set_arrow_create(
        cJSON * data,
        SF_COLUMN_DESC * metadata,
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
    void result_set_arrow_destroy(result_set_arrow_t * rs);

    /**
     * Advances to the next column.
     *
     * @return true if next column exists, otherwise false.
     */
    bool result_set_arrow_next_column(result_set_arrow_t * rs);

    /**
     * Advances to the next row.
     *
     * @return true if next row exists, otherwise false.
     */
    bool result_set_arrow_next_row(result_set_arrow_t * rs);

    /**
     * Get the current column value as string.
     *
     * @return The current column as a string.
     */
    char * result_set_arrow_get_current_column_as_string(result_set_arrow_t * rs);

    /**
     * Get the current row value as string.
     *
     * @return The current row as a string.
     */
    char * result_set_arrow_get_current_row_as_string(result_set_arrow_t * rs);


#ifdef __cplusplus
}
#endif

#endif // SNOWFLAKE_RESULTSETARROW_H
