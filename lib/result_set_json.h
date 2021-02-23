/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_RESULTSETJSON_H
#define SNOWFLAKE_RESULTSETJSON_H

#include "cJSON.h"
#include "snowflake/basic_types.h"
#include "snowflake/client.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * A result set interface for JSON result format.
     *
     * @see cpp/lib/ResultSet.hpp
     * @see cpp/lib/ResultSetJson.hpp
     */
    typedef struct rs_json {
        void * rs_object;
    } rs_json_t;

    /**
     * Parameterized constructor.
     * Initializes the result set with required information as well as data.
     *
     * @param initial_chunk             A pointer to the initial chunk.
     * @param metadata                  A pointer to the metadata for the result set.
     * @param tz_string                 The time zone.
     */
    rs_json_t * rs_json_create(
        cJSON * initial_chunk,
        SF_COLUMN_DESC * metadata,
        const char * tz_string);

    /**
     * Destructor.
     */
    void rs_json_destroy(rs_json_t * rs);

    /**
     * Appends the given chunk to the internal result set.
     *
     * @param rs                   The ResultSetJson object.
     * @param chunk                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_append_chunk(rs_json_t * rs, cJSON * chunk);

    /**
     * Resets the internal indices so that they may be used to traverse
     * the finished result set for consumption.
     *
     * @param rs                   The ResultSetJson object.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_finish_result_set(rs_json_t * rs);

    /**
     * Advances to next column.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_next_column(rs_json_t * rs);

    /**
     * Advances to next row.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_next_row(rs_json_t * rs);

    /**
     * Writes the value of the current cell as a boolean to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_bool(
        rs_json_t * rs,
        sf_bool * out_data);

    /**
     * Writes the value of the current cell as an int8 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_int8(
        rs_json_t * rs,
        int8 * out_data);

    /**
     * Writes the value of the current cell as an int32 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_int32(
        rs_json_t * rs,
        int32 * out_data);

    /**
     * Writes the value of the current cell as an int64 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_int64(
        rs_json_t * rs,
        int64 * out_data);

    /**
     * Writes the value of the current cell as a uint8 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_uint8(
        rs_json_t * rs,
        uint8 * out_data);

    /**
     * Writes the value of the current cell as a uint32 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_uint32(
        rs_json_t * rs,
        uint32 * out_data);

    /**
     * Writes the value of the current cell as a uint64 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_uint64(
        rs_json_t * rs,
        uint64 * out_data);

    /**
     * Writes the value of the current cell as a float32 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_float32(
        rs_json_t * rs,
        float32 * out_data);

    /**
     * Writes the value of the current cell as a float64 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_float64(
        rs_json_t * rs,
        float64 * out_data);

    /**
     * Writes the value of the current cell as a constant C-string to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_const_string(
        rs_json_t * rs,
        const char ** out_data);

    /**
     * Writes the value of the current cell as a C-string to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
     * @param io_len               The length of the requested string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_string(
        rs_json_t * rs,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Writes the value of the current cell as a timestamp to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param out_data             The buffer to write to.
    *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_curr_cell_as_timestamp(
        rs_json_t * rs,
        SF_TIMESTAMP * out_data);

    /**
     * Get the data in the current row as a cJSON struct.
     *
     * @return The current row as a cJSON struct.
     */
    cJSON * rs_json_get_curr_row(rs_json_t * rs);

    /**
     * Gets the number of rows in the current chunk being processed.
     *
     * @param rs                   The ResultSetJson object.
     *
     * @return the number of rows in the current chunk.
     */
    size_t rs_json_get_row_count_in_chunk(rs_json_t * rs);

    /**
     * Gets the total number of rows in the entire result set.
     *
     * @param rs                   The ResultSetJson object.
     *
     * @return the number of rows in the result set.
     */
    size_t rs_json_get_total_row_count(rs_json_t * rs);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_RESULTSETJSON_H
