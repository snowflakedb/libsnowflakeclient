/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_RESULTSETARROW_H
#define SNOWFLAKE_RESULTSETARROW_H

#include "cJSON.h"
#include "snowflake/basic_types.h"
#include "snowflake/client.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * A result set interface for Arrow result format.
     *
     * @see cpp/lib/ResultSet.hpp
     * @see cpp/lib/ResultSetArrow.hpp
     */
    typedef struct rs_arrow {
        void * rs_object;
    } rs_arrow_t;

    /**
     * Parameterized constructor.
     * Initializes the result set with required information as well as data.
     *
     * @param initial_chunk             A pointer to the result set data.
     * @param metadata                  A pointer to the metadata for the result set.
     * @param tz_string                 The time zone.
     */
    rs_arrow_t * rs_arrow_create(
        cJSON * initial_chunk,
        SF_COLUMN_DESC * metadata,
        const char * tz_string);

    /**
     * Destructor.
     */
    void rs_arrow_destroy(rs_arrow_t * rs);

    /**
     * Appends the given chunk to the internal result set.
     *
     * @param rs                   The ResultSetArrow object.
     * @param chunk                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_append_chunk(rs_arrow_t * rs, cJSON * chunk);

    /**
     * Resets the internal indices so that they may be used to traverse
     * the finished result set for consumption.
     *
     * @param rs                   The ResultSetArrow object.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_finish_result_set(rs_arrow_t * rs);

    /**
     * Advances to the next column.
     *
     * @param rs                   The ResultSetArrow object.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_next_column(rs_arrow_t * rs);

    /**
     * Advances to the next row.
     *
     * @param rs                   The ResultSetArrow object.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_next_row(rs_arrow_t * rs);

    /**
     * Writes the value of the current cell as a boolean to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_bool(
        rs_arrow_t * rs,
        sf_bool * out_data);

    /**
     * Writes the value of the current cell as an int8 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_int8(
        rs_arrow_t * rs,
        int8 * out_data);

    /**
     * Writes the value of the current cell as an int32 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_int32(
        rs_arrow_t * rs,
        int32 * out_data);

    /**
     * Writes the value of the current cell as an int64 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_int64(
        rs_arrow_t * rs,
        int64 * out_data);

    /**
     * Writes the value of the current cell as a uint8 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_uint8(
        rs_arrow_t * rs,
        uint8 * out_data);

    /**
     * Writes the value of the current cell as a uint32 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_uint32(
        rs_arrow_t * rs,
        uint32 * out_data);

    /**
     * Writes the value of the current cell as a uint64 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_uint64(
        rs_arrow_t * rs,
        uint64 * out_data);

    /**
     * Writes the value of the current cell as a float32 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_float32(
        rs_arrow_t * rs,
        float32 * out_data);

    /**
     * Writes the value of the current cell as a float64 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_float64(
        rs_arrow_t * rs,
        float64 * out_data);

    /**
     * Writes the value of the current cell as a constant C-string to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_const_string(
        rs_arrow_t * rs,
        const char ** out_data);

    /**
     * Writes the value of the current cell as a C-string to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
     * @param io_len               The length of the requested string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_string(
        rs_arrow_t * rs,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Writes the value of the current cell as a timestamp to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param out_data             The buffer to write to.
    *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_curr_cell_as_timestamp(
        rs_arrow_t * rs,
        SF_TIMESTAMP * out_data);

    /**
     * Gets the number of rows in the current chunk being processed.
     *
     * @param rs                   The ResultSetArrow object.
     *
     * @return the number of rows in the current chunk.
     */
    size_t rs_arrow_get_row_count_in_chunk(rs_arrow_t * rs);

    /**
     * Gets the total number of rows in the entire result set.
     *
     * @param rs                   The ResultSetArrow object.
     *
     * @return the number of rows in the result set.
     */
    size_t rs_arrow_get_total_row_count(rs_arrow_t * rs);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_RESULTSETARROW_H
