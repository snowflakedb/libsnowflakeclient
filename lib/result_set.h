#ifndef SNOWFLAKE_RESULTSET_H
#define SNOWFLAKE_RESULTSET_H

#include "snowflake/client.h"
#include "connection.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Result Set API ==============================================================================

    /**
     * Parameterized constructor.
     * Initializes the result set with required information as well as data.
     *
     * @param json_rowset               A pointer to the result set data.
     * @param metadata                  A pointer to the metadata for the result set.
     * @param query_result_format       The query result format.
     * @param tz_string                 The time zone.
     *
     * @return the created result set.
     */
    result_set_ptr rs_create_with_json_result(
        cJSON * json_rowset,
        SF_COLUMN_DESC * metadata,
        QueryResultFormat query_result_format,
        const char * tz_string);

    /**
     * Parameterized constructor.
     * Initializes the result set with required information as well as data.
     *
     * @param initial_chunk             A pointer to the initial chunk.
     * @param metadata                  A pointer to the metadata for the result set.
     * @param query_result_format       The query result format.
     * @param tz_string                 The time zone.
     *
     * @return the created result set.
     */
    result_set_ptr rs_create_with_chunk(
        void * initial_chunk,
        SF_COLUMN_DESC * metadata,
        QueryResultFormat query_result_format,
        const char * tz_string);

    /**
     * Destructor.
     *
     * @param rs                   The ResultSet object.
     */
    void rs_destroy(result_set_ptr rs);

    /**
     * Appends the given chunk to the internal result set.
     *
     * @param rs                   The ResultSet object.
     * @param chunk                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL
    rs_append_chunk(result_set_ptr rs, void * chunk);

    /**
     * Advances to the next row.
     *
     * @param rs                   The ResultSet object.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_next(result_set_ptr rs);

    /**
     * Writes the value of the current cell as a boolean to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_bool(
        result_set_ptr rs,
        size_t idx,
        sf_bool * out_data);

    /**
     * Writes the value of the current cell as an int8 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_int8(
        result_set_ptr rs,
        size_t idx,
        int8 * out_data);

    /**
     * Writes the value of the current cell as an int32 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_int32(
        result_set_ptr rs,
        size_t idx,
        int32 * out_data);

    /**
     * Writes the value of the current cell as an int64 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_int64(
        result_set_ptr rs,
        size_t idx,
        int64 * out_data);

    /**
     * Writes the value of the current cell as a uint8 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_uint8(
        result_set_ptr rs,
        size_t idx,
        uint8 * out_data);

    /**
     * Writes the value of the current cell as a uint32 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_uint32(
        result_set_ptr rs,
        size_t idx,
        uint32 * out_data);

    /**
     * Writes the value of the current cell as a uint64 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_uint64(
        result_set_ptr rs,
        size_t idx,
        uint64 * out_data);

    /**
     * Writes the value of the current cell as a float32 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_float32(
        result_set_ptr rs,
        size_t idx,
        float32 * out_data);

    /**
     * Writes the value of the current cell as a float64 to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_float64(
        result_set_ptr rs,
        size_t idx,
        float64 * out_data);

    /**
     * Writes the value of the current cell as a constant C-string to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_const_string(
        result_set_ptr rs,
        size_t idx,
        const char ** out_data);

    /**
     * Writes the value of the current cell as a timestamp to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_as_timestamp(
        result_set_ptr rs,
        size_t idx,
        SF_TIMESTAMP * out_data);

    /**
     * Writes the length of the current cell to the provided buffer.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_get_cell_strlen(
        result_set_ptr rs,
        size_t idx,
        size_t * out_data);

    /**
     * Gets the number of rows in the current chunk being processed.
     *
     * @param rs                   The ResultSet object.
     *
     * @return the number of rows in the current chunk.
     */
    size_t rs_get_row_count_in_chunk(result_set_ptr rs);

    /**
     * Indiciates whether the current cell is null or not.
     *
     * @param rs                   The ResultSet object.
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL rs_is_cell_null(
        result_set_ptr rs,
        size_t idx,
        sf_bool * out_data);

    /**
    * Get the latest error code.
    *
    * @param rs                   The ResultSet object.
    *
    * @return the latest error code. 0 if no error.
    */
    SF_STATUS STDCALL rs_get_error(result_set_ptr rs);

    /**
    * Get the latest error code.
    *
    * @param rs                   The ResultSet object.
    *
    * @return the latest error message. empty string if no error.
    */
    const char* rs_get_error_message(result_set_ptr rs);

    // return callback struct for arrow chunk downloading
    NON_JSON_RESP* callback_create_arrow_resp(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_RESULTSETJSON_H
