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
     * @param rowset                    A pointer to the result set data.
     * @param metadata                  A pointer to the metadata for the result set.
     * @param tz_string                 The time zone.
     */
    rs_json_t * rs_json_create(
        cJSON * rowset,
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
     * Advances to next cell.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_next(rs_json_t * rs);

    /**
     * Writes the value of the current cell as a boolean to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_bool(
        rs_json_t * rs,
        size_t idx,
        sf_bool * out_data);

    /**
     * Writes the value of the current cell as an int8 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_int8(
        rs_json_t * rs,
        size_t idx,
        int8 * out_data);

    /**
     * Writes the value of the current cell as an int32 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_int32(
        rs_json_t * rs,
        size_t idx,
        int32 * out_data);

    /**
     * Writes the value of the current cell as an int64 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_int64(
        rs_json_t * rs,
        size_t idx,
        int64 * out_data);

    /**
     * Writes the value of the current cell as a uint8 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_uint8(
        rs_json_t * rs,
        size_t idx,
        uint8 * out_data);

    /**
     * Writes the value of the current cell as a uint32 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_uint32(
        rs_json_t * rs,
        size_t idx,
        uint32 * out_data);

    /**
     * Writes the value of the current cell as a uint64 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_uint64(
        rs_json_t * rs,
        size_t idx,
        uint64 * out_data);

    /**
     * Writes the value of the current cell as a float32 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_float32(
        rs_json_t * rs,
        size_t idx,
        float32 * out_data);

    /**
     * Writes the value of the current cell as a float64 to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_float64(
        rs_json_t * rs,
        size_t idx,
        float64 * out_data);

    /**
     * Writes the value of the current cell as a constant C-string to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_const_string(
        rs_json_t * rs,
        size_t idx,
        const char ** out_data);

    /**
     * Writes the value of the current cell as a C-string to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     * @param io_len               The length of the requested string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_string(
        rs_json_t * rs,
        size_t idx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Writes the value of the current cell as a timestamp to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
    *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_as_timestamp(
        rs_json_t * rs,
        size_t idx,
        SF_TIMESTAMP * out_data);

    /**
     * Writes the length of the current cell to the provided buffer.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_get_cell_strlen(rs_json_t * rs, size_t idx, size_t * out_data);

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

    /**
     * Indiciates whether the current cell is null or not.
     *
     * @param rs                   The ResultSetJson object.
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_json_is_cell_null(rs_json_t * rs, size_t idx, sf_bool * out_data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_RESULTSETJSON_H
