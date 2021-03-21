/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_RESULTSETARROW_H
#define SNOWFLAKE_RESULTSETARROW_H

#include "cJSON.h"
#include "snowflake/basic_types.h"
#include "snowflake/client.h"
#include "connection.h"

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
     * Initializes the result set with rowset64 data in json result set.
     *
     * @param json_rowset64             A pointer to the rowset64 data in json result set.
     * @param metadata                  A pointer to the metadata for the result set.
     * @param tz_string                 The time zone.
     */
    rs_arrow_t * rs_arrow_create_with_json_result(
        cJSON * json_rowset64,
        SF_COLUMN_DESC * metadata,
        const char * tz_string);

    /**
    * Parameterized constructor.
    * Initializes the result set with the first arrow chunk of the result set.
    *
    * @param initial_chunk             A pointer to the first chunk of the result set.
    * @param metadata                  A pointer to the metadata for the result set.
    * @param tz_string                 The time zone.
    */
    rs_arrow_t * rs_arrow_create_with_chunk(
      NON_JSON_RESP * initial_chunk,
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
    SF_STATUS STDCALL rs_arrow_append_chunk(rs_arrow_t * rs, NON_JSON_RESP * chunk);

    /**
     * Advances to the next row.
     *
     * @param rs                   The ResultSetArrow object.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_next(rs_arrow_t * rs);

    /**
     * Writes the value of the current cell as a boolean to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_bool(
        rs_arrow_t * rs,
        size_t idx,
        sf_bool * out_data);

    /**
     * Writes the value of the current cell as an int8 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_int8(
        rs_arrow_t * rs,
        size_t idx,
        int8 * out_data);

    /**
     * Writes the value of the current cell as an int32 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_int32(
        rs_arrow_t * rs,
        size_t idx,
        int32 * out_data);

    /**
     * Writes the value of the current cell as an int64 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_int64(
        rs_arrow_t * rs,
        size_t idx,
        int64 * out_data);

    /**
     * Writes the value of the current cell as a uint8 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_uint8(
        rs_arrow_t * rs,
        size_t idx,
        uint8 * out_data);

    /**
     * Writes the value of the current cell as a uint32 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_uint32(
        rs_arrow_t * rs,
        size_t idx,
        uint32 * out_data);

    /**
     * Writes the value of the current cell as a uint64 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_uint64(
        rs_arrow_t * rs,
        size_t idx,
        uint64 * out_data);

    /**
     * Writes the value of the current cell as a float32 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_float32(
        rs_arrow_t * rs,
        size_t idx,
        float32 * out_data);

    /**
     * Writes the value of the current cell as a float64 to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_float64(
        rs_arrow_t * rs,
        size_t idx,
        float64 * out_data);

    /**
     * Writes the value of the current cell as a constant C-string to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_const_string(
        rs_arrow_t * rs,
        size_t idx,
        const char ** out_data);

    /**
     * Writes the value of the current cell as a C-string to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     * @param io_len               The length of the requested string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_string(
        rs_arrow_t * rs,
        size_t idx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Writes the value of the current cell as a timestamp to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
    *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_as_timestamp(
        rs_arrow_t * rs,
        size_t idx,
        SF_TIMESTAMP * out_data);

    /**
     * Writes the length of the current cell to the provided buffer.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_get_cell_strlen(rs_arrow_t * rs, size_t idx, size_t * out_data);

    /**
     * Gets the number of rows in the current chunk being processed.
     *
     * @param rs                   The ResultSetArrow object.
     *
     * @return the number of rows in the current chunk.
     */
    size_t rs_arrow_get_row_count_in_chunk(rs_arrow_t * rs);

    /**
     * Indiciates whether the current cell is null or not.
     *
     * @param rs                   The ResultSetArrow object.
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_arrow_is_cell_null(rs_arrow_t * rs, size_t idx, sf_bool * out_data);

    NON_JSON_RESP* callback_create_arrow_resp(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_RESULTSETARROW_H
