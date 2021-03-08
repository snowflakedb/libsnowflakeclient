/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_RESULTSET_H
#define SNOWFLAKE_RESULTSET_H

#include "snowflake/client.h"
#include "result_set_arrow.h"
#include "result_set_json.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Utility =====================================================================================

    /**
     * An enumeration over all supported query result formats.
     * This is used to help deciding which result set to create.
     *
     * NOTE: Keep the order consistent with rowset_key_map!
     */
    typedef enum QueryResultFormat
    {
        ARROW_FORMAT, JSON_FORMAT, FORMAT_MAX
    } QueryResultFormat_t;

    // Result Set API ==============================================================================

    /**
     * Parameterized constructor.
     * Initializes the result set with required information as well as data.
     *
     * @param initial_chunk             A pointer to the result set data.
     * @param metadata                  A pointer to the metadata for the result set.
     * @param query_result_format       The query result format.
     * @param tz_string                 The time zone.
     *
     * @return the created result set.
     */
    void * rs_create(
        cJSON * initial_chunk,
        SF_COLUMN_DESC * metadata,
        QueryResultFormat_t * query_result_format,
        const char * tz_string);

    /**
     * Destructor.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     */
    void rs_destroy(void * rs, QueryResultFormat_t * query_result_format);

    /**
     * Appends the given chunk to the internal result set.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param chunk                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL
    rs_append_chunk(void * rs, QueryResultFormat_t * query_result_format, cJSON * chunk);

    /**
     * Resets the internal indices so that they may be used to traverse
     * the finished result set for consumption.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_finish_result_set(void * rs, QueryResultFormat_t * query_result_format);

    /**
     * Advances to the next cell.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_next(void * rs, QueryResultFormat_t * query_result_format);

    /**
     * Writes the value of the current cell as a boolean to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_bool(
        void * rs,
        QueryResultFormat_t * query_result_format,
        sf_bool * out_data);

    /**
     * Writes the value of the current cell as an int8 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_int8(
        void * rs,
        QueryResultFormat_t * query_result_format,
        int8 * out_data);

    /**
     * Writes the value of the current cell as an int32 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_int32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        int32 * out_data);

    /**
     * Writes the value of the current cell as an int64 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_int64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        int64 * out_data);

    /**
     * Writes the value of the current cell as a uint8 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_uint8(
        void * rs,
        QueryResultFormat_t * query_result_format,
        uint8 * out_data);

    /**
     * Writes the value of the current cell as a uint32 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_uint32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        uint32 * out_data);

    /**
     * Writes the value of the current cell as a uint64 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_uint64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        uint64 * out_data);

    /**
     * Writes the value of the current cell as a float32 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_float32(
        void * rs,
        QueryResultFormat_t * query_result_format,
        float32 * out_data);

    /**
     * Writes the value of the current cell as a float64 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_float64(
        void * rs,
        QueryResultFormat_t * query_result_format,
        float64 * out_data);

    /**
     * Writes the value of the current cell as a constant C-string to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_const_string(
        void * rs,
        QueryResultFormat_t * query_result_format,
        const char ** out_data);

    /**
     * Writes the value of the current cell as a C-string to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     * @param io_len               The length of the requested string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_string(
        void * rs,
        QueryResultFormat_t * query_result_format,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Writes the value of the current cell as a timestamp to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_as_timestamp(
        void * rs,
        QueryResultFormat_t * query_result_format,
        SF_TIMESTAMP * out_data);

    /**
     * Writes the length of the current cell to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_curr_cell_strlen(
        void * rs,
        QueryResultFormat_t * query_result_format,
        size_t * out_data);

    /**
     * Get the data in the current row as a cJSON struct.
     *
     * Only valid if the query result format is JSON_FORMAT.
     *
     * @return The current row as a cJSON struct if JSON_FORMAT, otherwise NULL.
     */
    cJSON * rs_get_curr_row(void * rs, QueryResultFormat_t * query_result_format);

    /**
     * Gets the number of rows in the current chunk being processed.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     *
     * @return the number of rows in the current chunk.
     */
    size_t rs_get_row_count_in_chunk(void * rs, QueryResultFormat_t * query_result_format);

    /**
     * Gets the total number of rows in the entire result set.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     *
     * @return the number of rows in the result set.
     */
    size_t rs_get_total_row_count(void * rs, QueryResultFormat_t * query_result_format);

    /**
     * Indiciates whether the current cell is null or not.
     *
     * @param rs                   The ResultSet object.
     * @param query_result_format  The query result format.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_is_curr_cell_null(
        void * rs,
        QueryResultFormat_t * query_result_format,
        sf_bool * out_data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_RESULTSETJSON_H
