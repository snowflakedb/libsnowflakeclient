/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <assert.h>
#include "utils/test_setup.h"

void test_column_as_boolean_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 1, 0, 'some string', '', "
                            "to_boolean('yes'), to_boolean('no'), "
                            "0.000001, 0.0, NULL, "
                            "as_double(1.79769e+308), as_double(2.225074e-308), "
                            "32767, -32768, 2147483647, -2147483648, "
                            "9223372036854775807, -9223372036854775808;", 0);

    // Stores the result from the fetch operation
    sf_bool out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Should be true
        if (snowflake_column_as_boolean(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        // Should be false
        if (snowflake_column_as_boolean(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_FALSE);

        // Should be true
        if (snowflake_column_as_boolean(sfstmt, 3, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        // Should be false
        if (snowflake_column_as_boolean(sfstmt, 4, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_FALSE);

        // Should be true
        if (snowflake_column_as_boolean(sfstmt, 5, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        // Should be false
        if (snowflake_column_as_boolean(sfstmt, 6, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_FALSE);

        // Should be true
        if (snowflake_column_as_boolean(sfstmt, 7, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        // Should be false
        if (snowflake_column_as_boolean(sfstmt, 8, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_FALSE);

        // Should be false
        if (snowflake_column_as_boolean(sfstmt, 9, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_FALSE);

        // Conversion from DOUBLE to BOOL
        if (snowflake_column_as_boolean(sfstmt, 10, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        if (snowflake_column_as_boolean(sfstmt, 11, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        // Conversion from INT16 to BOOL
        if (snowflake_column_as_boolean(sfstmt, 12, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        if (snowflake_column_as_boolean(sfstmt, 13, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        // Conversion from INT32 to BOOL
        if (snowflake_column_as_boolean(sfstmt, 14, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        if (snowflake_column_as_boolean(sfstmt, 15, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        // Conversion from INT64 to BOOL
        if (snowflake_column_as_boolean(sfstmt, 16, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        if (snowflake_column_as_boolean(sfstmt, 17, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, SF_BOOLEAN_TRUE);

        // Out of bounds column check
        if (!(status = snowflake_column_as_boolean(sfstmt, 18, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_boolean(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error)); 
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_int8_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select to_char('s'), '', NULL", 0);

    // Stores the result from the fetch operation
    int8 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case with a non-empty string
        if (snowflake_column_as_int8(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 115);

        // Case with an empty string
        if (snowflake_column_as_int8(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0);

        // Case with a null column
        if (snowflake_column_as_int8(sfstmt, 3, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0);

        // Out of bounds check
        if (!(status = snowflake_column_as_int8(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_int8(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_int32_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 1, 0, 2147483647, -2147483648, "
                            "10.01, NULL, 'some string', 10000000000000000000, "
                            "-10000000000000000000", 0);

    // Stores the result from the fetch operation
    int32 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case
        if (snowflake_column_as_int32(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 1);

        // Case where valid conversion leads to a 0
        if (snowflake_column_as_int32(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0);

        // Case where a valid conversion leads to the max value for an int32
        if ((status = snowflake_column_as_int32(sfstmt, 3, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert(out == SF_INT32_MAX);

        // Case where a valid conversion leads to the min value for an int32
        if ((status = snowflake_column_as_int32(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert(out == SF_INT32_MIN);

        // Trying to convert a float to an int
        if (snowflake_column_as_int32(sfstmt, 5, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(10, out);

        // Trying to convert a NULL to an int
        if (snowflake_column_as_int32(sfstmt, 6, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(0, out);

        // **************************************************
        // Start of invalid conversions
        // **************************************************

        // Trying to convert non-numerical string to an int
        if (!(status = snowflake_column_as_int32(sfstmt, 7, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_CONVERSION_FAILURE);

        // Trying to convert a number that is out of range for int32
        if (!(status = snowflake_column_as_int32(sfstmt, 8, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_RANGE);

        // Trying to convert a number that is out of range for int32
        if (!(status = snowflake_column_as_int32(sfstmt, 9, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_RANGE);

        // Out of bounds check
        if (!(status = snowflake_column_as_int32(sfstmt, 10, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_int32(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_int64_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 1, 0, 9223372036854775807, -9223372036854775808, "
                            "10.01, NULL, 'some string', 10000000000000000000, "
                            "-10000000000000000000", 0);

    // Stores the result from the fetch operation
    int64 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case
        if (snowflake_column_as_int64(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 1);

        // Case where valid conversion leads to a 0
        if (snowflake_column_as_int64(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0);

        // Case where a valid conversion leads to the max value for an int64
        if ((status = snowflake_column_as_int64(sfstmt, 3, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert(out == SF_INT64_MAX);

        // Case where a valid conversion leads to the min value for an int64
        if ((status = snowflake_column_as_int64(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert(out == SF_INT64_MIN);

        // Trying to convert a float to an int
        if (snowflake_column_as_int64(sfstmt, 5, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(10, out);

        // Trying to convert a NULL to an int
        if (snowflake_column_as_int64(sfstmt, 6, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(0, out);

        // **************************************************
        // Start of invalid conversions
        // **************************************************

        // Trying to convert non-numerical string to an int
        if (!(status = snowflake_column_as_int64(sfstmt, 7, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_CONVERSION_FAILURE);

        // Trying to convert a number that is out of range for int64
        if (!(status = snowflake_column_as_int64(sfstmt, 8, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_RANGE);

        // Trying to convert a number that is out of range for int64
        if (!(status = snowflake_column_as_int64(sfstmt, 9, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_RANGE);

        // Out of bounds check
        if (!(status = snowflake_column_as_int64(sfstmt, 10, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_int64(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_uint8_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select to_char('s'), '', NULL", 0);

    // Stores the result from the fetch operation
    uint8 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case with a non-empty string
        if (snowflake_column_as_uint8(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 115);

        // Case with an empty string
        if (snowflake_column_as_uint8(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0);

        // Case with a null column
        if (snowflake_column_as_uint8(sfstmt, 3, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0);

        // Out of bounds check
        if (!(status = snowflake_column_as_uint8(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_uint8(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_uint32_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 1, 0, 4294967295, -1, 10.01, NULL, "
                            "'some string', 20000000000000000000", 0);

    // Stores the result from the fetch operation
    uint32 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case
        if (snowflake_column_as_uint32(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 1);

        // Case where valid conversion leads to a 0
        if (snowflake_column_as_uint32(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 0);

        // Case where a valid conversion leads to the max value for an uint32
        if ((status = snowflake_column_as_uint32(sfstmt, 3, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert(out == SF_UINT32_MAX);

        // Trying to convert a negative number to an uint32
        if ((status = snowflake_column_as_uint32(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert(out == SF_UINT32_MAX);

        // Trying to convert a float to an uint32
        if (snowflake_column_as_uint32(sfstmt, 5, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(10, out);

        // Trying to convert a NULL to an uint32
        if (snowflake_column_as_uint32(sfstmt, 6, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(0, out);

        // **************************************************
        // Start of invalid conversions
        // **************************************************

        // Trying to convert non-numerical string to an uint32
        if (!(status = snowflake_column_as_uint32(sfstmt, 7, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_CONVERSION_FAILURE);

        // Trying to convert a number that is out of range for uint32
        if (!(status = snowflake_column_as_uint32(sfstmt, 8, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_RANGE);

        // Out of bounds check
        if (!(status = snowflake_column_as_uint32(sfstmt, 9, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_uint32(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_uint64_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 1, 0, 18446744073709551615, -1, 10.01, NULL, "
                            "'some string', 20000000000000000000", 0);

    // Stores the result from the fetch operation
    uint64 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case
        if (snowflake_column_as_uint64(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 1);

        // Case where valid conversion leads to a 0
        if (snowflake_column_as_uint64(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 0);

        // Case where a valid conversion leads to the max value for an uint64
        if ((status = snowflake_column_as_uint64(sfstmt, 3, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert(out == SF_UINT64_MAX);

        // Trying to convert a negative number to an uint64
        if ((status = snowflake_column_as_uint64(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert(out == SF_UINT64_MAX);

        // Trying to convert a float to an uint64
        if (snowflake_column_as_uint64(sfstmt, 5, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(10, out);

        // Trying to convert a NULL to an uint64
        if (snowflake_column_as_uint64(sfstmt, 6, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(0, out);

        // **************************************************
        // Start of invalid conversions
        // **************************************************

        // Trying to convert non-numerical string to an uint64
        if (!(status = snowflake_column_as_uint64(sfstmt, 7, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_CONVERSION_FAILURE);

        // Trying to convert a number that is out of range for uint64
        if (!(status = snowflake_column_as_uint64(sfstmt, 8, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_RANGE);

        // Out of bounds check
        if (!(status = snowflake_column_as_uint64(sfstmt, 9, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_uint64(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_float32_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 1, 0, 1.1, as_double(3.40282e+38),"
                            "as_double(1.176e-38), NULL, 'some string', "
                            "2e500", 0);

    // Stores the result from the fetch operation
    float32 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case to convert an int
        if (snowflake_column_as_float32(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 1.0);

        // Case where valid conversion leads to a 0
        if (snowflake_column_as_float32(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 0.0);

        // Case where string to float conversion is not perfect
        if (snowflake_column_as_float32(sfstmt, 3, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == (float32) 1.1);

        // Case where a valid conversion leads to the max value for a float32
        if ((status = snowflake_column_as_float32(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Case where a valid conversion leads to the min positive value for a float32
        if ((status = snowflake_column_as_float32(sfstmt, 5, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Case where a valid conversion leads to the min positive value for a float32
        if ((status = snowflake_column_as_float32(sfstmt, 6, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0.0);

        // **************************************************
        // Start of invalid conversions
        // **************************************************

        // Trying to convert non-numerical string to an float32
        if (!(status = snowflake_column_as_float32(sfstmt, 7, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_CONVERSION_FAILURE);

        // Trying to convert a number that is out of range for float32
        if (!(status = snowflake_column_as_float32(sfstmt, 8, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_RANGE);

        // Out of bounds check
        if (!(status = snowflake_column_as_float32(sfstmt, 9, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_float32(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_float64_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 1, 0, 1.1, as_double(1.79769e+308),"
                            "as_double(2.225074e-308), NULL, 'some string', "
                            "2e500", 0);

    // Stores the result from the fetch operation
    float64 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case to convert an int
        if (snowflake_column_as_float64(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 1.0);

        // Case where valid conversion leads to a 0
        if (snowflake_column_as_float64(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 0.0);

        // Basic Case
        if (snowflake_column_as_float64(sfstmt, 3, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == 1.1);

        // Case where a valid conversion leads to the max value for a float64
        if ((status = snowflake_column_as_float64(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Case where a valid conversion leads to the min positive value for a float64
        if ((status = snowflake_column_as_float64(sfstmt, 5, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Case where a valid conversion leads to the min positive value for a float64
        if ((status = snowflake_column_as_float64(sfstmt, 6, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0.0);

        // **************************************************
        // Start of invalid conversions
        // **************************************************

        // Trying to convert non-numerical string to an float64
        if (!(status = snowflake_column_as_float64(sfstmt, 7, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_CONVERSION_FAILURE);

        // Trying to convert a number that is out of range for float64
        if (!(status = snowflake_column_as_float64(sfstmt, 8, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_RANGE);

        // Out of bounds check
        if (!(status = snowflake_column_as_float64(sfstmt, 9, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_float64(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_timestamp_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "alter session set timezone='America/Los_Angeles'", 0);

    snowflake_query(sfstmt, "select timestamp_ltz_from_parts(2018, 06, 01, 7, 40, 52, 968746000),"
                            "timestamp_ntz_from_parts(2018, 06, 01, 14, 40, 52, 968746000),"
                            "timestamp_tz_from_parts(2018, 06, 01, 10, 40, 52, 968746000, 'America/New_York'), "
                            "NULL, "
                            "to_timestamp_tz('2018-10-10 12:34:56 -7:00'), "
                            "to_timestamp_tz('2018-10-10 20:34:56 +1:00'), "
                            "to_timestamp_ltz('2018-10-10 14:34:56 -5:00'), "
                            "to_timestamp_ntz('2018-10-10 19:34:56 +6:00')", 0);

    // Stores the result from the fetch operation
    SF_TIMESTAMP out[8];

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Timestamp LTZ case
        if (snowflake_column_as_timestamp(sfstmt, 1, &out[0])) {
            dump_error(&(sfstmt->error));
        }

        // Timestamp NTZ case
        if (snowflake_column_as_timestamp(sfstmt, 2, &out[1])) {
            dump_error(&(sfstmt->error));
        }

        // Timestamp TZ case
        if (snowflake_column_as_timestamp(sfstmt, 3, &out[2])) {
            dump_error(&(sfstmt->error));
        }

        for (int i = 0; i < 3; i++) {
            assert_int_equal(2018, snowflake_timestamp_get_year(&out[i]));
            assert_int_equal(6, snowflake_timestamp_get_month(&out[i]));
            assert_int_equal(1, snowflake_timestamp_get_mday(&out[i]));
            assert_int_equal(5, snowflake_timestamp_get_wday(&out[i]));
            assert_int_equal(151, snowflake_timestamp_get_yday(&out[i]));
            // The hours are different based on the timestamp type
            if (out[i].ts_type == SF_DB_TYPE_TIMESTAMP_LTZ) {
                assert_int_equal(7, snowflake_timestamp_get_hours(&out[i]));
            } else if (out[i].ts_type == SF_DB_TYPE_TIMESTAMP_NTZ) {
                assert_int_equal(14, snowflake_timestamp_get_hours(&out[i]));
            } else {
                assert_int_equal(10, snowflake_timestamp_get_hours(&out[i]));
            }
            assert_int_equal(40, snowflake_timestamp_get_minutes(&out[i]));
            assert_int_equal(52, snowflake_timestamp_get_seconds(&out[i]));
            assert_int_equal(968746000, snowflake_timestamp_get_nanoseconds(&out[i]));
            assert_int_equal(9, snowflake_timestamp_get_scale(&out[i]));
            time_t epoch_time = 0;
            snowflake_timestamp_get_epoch_seconds(&out[i], &epoch_time);
            assert_int_equal(1527864052, epoch_time);
        }

        // NULL case, should be the epoch
        if (snowflake_column_as_timestamp(sfstmt, 4, &out[3])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(1970, snowflake_timestamp_get_year(&out[3]));
        assert_int_equal(1, snowflake_timestamp_get_month(&out[3]));
        assert_int_equal(1, snowflake_timestamp_get_mday(&out[3]));
        assert_int_equal(0, snowflake_timestamp_get_hours(&out[3]));
        assert_int_equal(0, snowflake_timestamp_get_minutes(&out[3]));
        assert_int_equal(0, snowflake_timestamp_get_seconds(&out[3]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[3]));
        time_t epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[3], &epoch_time);
        assert_int_equal(0, epoch_time);

        // TZ case with negative -07:00 offset
        if (snowflake_column_as_timestamp(sfstmt, 5, &out[4])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(2018, snowflake_timestamp_get_year(&out[4]));
        assert_int_equal(10, snowflake_timestamp_get_month(&out[4]));
        assert_int_equal(10, snowflake_timestamp_get_mday(&out[4]));
        assert_int_equal(12, snowflake_timestamp_get_hours(&out[4]));
        assert_int_equal(34, snowflake_timestamp_get_minutes(&out[4]));
        assert_int_equal(56, snowflake_timestamp_get_seconds(&out[4]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[4]));
        assert_int_equal(-7 * 60, snowflake_timestamp_get_tzoffset(&out[4]));
        epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[4], &epoch_time);
        assert_int_equal(1539200096, epoch_time);

        // TZ case with positive +01:00 offset
        if (snowflake_column_as_timestamp(sfstmt, 6, &out[5])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(2018, snowflake_timestamp_get_year(&out[5]));
        assert_int_equal(10, snowflake_timestamp_get_month(&out[5]));
        assert_int_equal(10, snowflake_timestamp_get_mday(&out[5]));
        assert_int_equal(20, snowflake_timestamp_get_hours(&out[5]));
        assert_int_equal(34, snowflake_timestamp_get_minutes(&out[5]));
        assert_int_equal(56, snowflake_timestamp_get_seconds(&out[5]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[5]));
        assert_int_equal(1 * 60, snowflake_timestamp_get_tzoffset(&out[5]));
        epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[5], &epoch_time);
        assert_int_equal(1539200096, epoch_time);

        // LTZ case with local timezone set to UTC
        if (snowflake_column_as_timestamp(sfstmt, 7, &out[6])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(2018, snowflake_timestamp_get_year(&out[6]));
        assert_int_equal(10, snowflake_timestamp_get_month(&out[6]));
        assert_int_equal(10, snowflake_timestamp_get_mday(&out[6]));
        assert_int_equal(12, snowflake_timestamp_get_hours(&out[6]));
        assert_int_equal(34, snowflake_timestamp_get_minutes(&out[6]));
        assert_int_equal(56, snowflake_timestamp_get_seconds(&out[6]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[6]));
        epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[6], &epoch_time);
        assert_int_equal(1539200096, epoch_time);

        // NTZ case
        if (snowflake_column_as_timestamp(sfstmt, 8, &out[7])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(2018, snowflake_timestamp_get_year(&out[7]));
        assert_int_equal(10, snowflake_timestamp_get_month(&out[7]));
        assert_int_equal(10, snowflake_timestamp_get_mday(&out[7]));
        assert_int_equal(19, snowflake_timestamp_get_hours(&out[7]));
        assert_int_equal(34, snowflake_timestamp_get_minutes(&out[7]));
        assert_int_equal(56, snowflake_timestamp_get_seconds(&out[7]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[7]));
        assert_int_equal(0, snowflake_timestamp_get_tzoffset(&out[7]));
        epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[7], &epoch_time);
        assert_int_equal(1539200096, epoch_time);

        // Out of bounds check
        if (!(status = snowflake_column_as_timestamp(sfstmt, 9, &out[3]))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_timestamp(sfstmt, -1, &out[3]))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_timestamp_windows_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select "
                            "NULL, "
                            "to_timestamp_tz('2018-10-10 12:34:56 -7:00'), "
                            "to_timestamp_tz('2018-10-10 20:34:56 +1:00'), "
                            "to_timestamp_ntz('2018-10-10 19:34:56 +6:00')", 0);

    // Stores the result from the fetch operation
    SF_TIMESTAMP out[4];

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // NULL case, should be the epoch
        if (snowflake_column_as_timestamp(sfstmt, 1, &out[0])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(1970, snowflake_timestamp_get_year(&out[0]));
        assert_int_equal(1, snowflake_timestamp_get_month(&out[0]));
        assert_int_equal(1, snowflake_timestamp_get_mday(&out[0]));
        assert_int_equal(0, snowflake_timestamp_get_hours(&out[0]));
        assert_int_equal(0, snowflake_timestamp_get_minutes(&out[0]));
        assert_int_equal(0, snowflake_timestamp_get_seconds(&out[0]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[0]));
        time_t epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[0], &epoch_time);
        assert_int_equal(0, epoch_time);

        // TZ case with negative -07:00 offset
        if (snowflake_column_as_timestamp(sfstmt, 2, &out[1])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(2018, snowflake_timestamp_get_year(&out[1]));
        assert_int_equal(10, snowflake_timestamp_get_month(&out[1]));
        assert_int_equal(10, snowflake_timestamp_get_mday(&out[1]));
        assert_int_equal(12, snowflake_timestamp_get_hours(&out[1]));
        assert_int_equal(34, snowflake_timestamp_get_minutes(&out[1]));
        assert_int_equal(56, snowflake_timestamp_get_seconds(&out[1]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[1]));
        assert_int_equal(-7 * 60, snowflake_timestamp_get_tzoffset(&out[1]));
        epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[1], &epoch_time);
        assert_int_equal(1539200096, epoch_time);

        // TZ case with positive +01:00 offset
        if (snowflake_column_as_timestamp(sfstmt, 3, &out[2])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(2018, snowflake_timestamp_get_year(&out[2]));
        assert_int_equal(10, snowflake_timestamp_get_month(&out[2]));
        assert_int_equal(10, snowflake_timestamp_get_mday(&out[2]));
        assert_int_equal(20, snowflake_timestamp_get_hours(&out[2]));
        assert_int_equal(34, snowflake_timestamp_get_minutes(&out[2]));
        assert_int_equal(56, snowflake_timestamp_get_seconds(&out[2]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[2]));
        assert_int_equal(1 * 60, snowflake_timestamp_get_tzoffset(&out[2]));
        epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[2], &epoch_time);
        assert_int_equal(1539200096, epoch_time);

        // NTZ case
        if (snowflake_column_as_timestamp(sfstmt, 4, &out[3])) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(2018, snowflake_timestamp_get_year(&out[3]));
        assert_int_equal(10, snowflake_timestamp_get_month(&out[3]));
        assert_int_equal(10, snowflake_timestamp_get_mday(&out[3]));
        assert_int_equal(19, snowflake_timestamp_get_hours(&out[3]));
        assert_int_equal(34, snowflake_timestamp_get_minutes(&out[3]));
        assert_int_equal(56, snowflake_timestamp_get_seconds(&out[3]));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&out[3]));
        assert_int_equal(0, snowflake_timestamp_get_tzoffset(&out[3]));
        epoch_time = 0;
        snowflake_timestamp_get_epoch_seconds(&out[3], &epoch_time);
        assert_int_equal(1539200096, epoch_time);

        // Out of bounds check
        if (!(status = snowflake_column_as_timestamp(sfstmt, 5, &out[3]))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_timestamp(sfstmt, -1, &out[3]))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_const_str_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 'some string that is not empty', '', "
                            "as_double(1.1), as_integer(to_variant(10)), "
                            "to_boolean(1), NULL", 0);

    // Stores the result from the fetch operation
    const char *out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case
        if (snowflake_column_as_const_str(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("some string that is not empty", out);

        // Case where string is empty
        if (snowflake_column_as_const_str(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("", out);

        // Case with a DOUBLE DB type
        if (snowflake_column_as_const_str(sfstmt, 3, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("1.1", out);

        // Case with a INTEGER DB type
        if (snowflake_column_as_const_str(sfstmt, 4, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("10", out);

        // Case with a BOOLEAN DB type
        if (snowflake_column_as_const_str(sfstmt, 5, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("1", out);

        // Get the value of a NULL column
        if (snowflake_column_as_const_str(sfstmt, 6, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert(out == NULL);

        // Out of bounds check
        if (!(status = snowflake_column_as_const_str(sfstmt, 7, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_const_str(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_is_null_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 1, NULL, to_boolean(0)", 0);;

    // Stores the result from the fetch operation
    sf_bool out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case (not null)
        if (snowflake_column_is_null(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_false(out);

        // Case where column is null
        if (snowflake_column_is_null(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_true(out);

        // Case where column is boolean false
        if (snowflake_column_is_null(sfstmt, 3, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_false(out);

        // Out of bounds check
        if (!(status = snowflake_column_is_null(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_is_null(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_strlen_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 'some string', '', NULL", 0);;

    // Stores the result from the fetch operation
    size_t out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case with a non-empty string
        if (snowflake_column_strlen(sfstmt, 1, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 11);

        // Case with an empty string
        if (snowflake_column_strlen(sfstmt, 2, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0);

        // Case with a null column
        if (snowflake_column_strlen(sfstmt, 3, &out)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(out, 0);

        // Out of bounds check
        if (!(status = snowflake_column_strlen(sfstmt, 4, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_strlen(sfstmt, -1, &out))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_str_helper(sf_bool use_arrow) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt,
                        use_arrow == SF_BOOLEAN_TRUE
                        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON");

    snowflake_query(sfstmt, "select 'some string', '', NULL, "
                            "to_boolean(0), to_boolean(1), "
                            "date_from_parts(2018, 09, 14), "
                            "timestamp_ltz_from_parts(2014, 03, 20, 15, 30, 45, 493679329), "
                            "timestamp_ntz_from_parts(2014, 03, 20, 15, 30, 45, 493679329), "
                            "timestamp_tz_from_parts(2014, 03, 20, 15, 30, 45, 493679329, 'America/Los_Angeles')", 0);

    // Stores the result from the fetch operation
    char *out = NULL;
    size_t out_len = 0;
    size_t out_max_size = 0;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Basic Case with a non-empty string
        if (snowflake_column_as_str(sfstmt, 1, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("some string", out);
        assert_int_equal(11, out_len);

        // Case with an empty string
        if (snowflake_column_as_str(sfstmt, 2, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("", out);
        assert_int_equal(0, out_len);

        // Case with a NULL value
        if (snowflake_column_as_str(sfstmt, 3, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("", out);
        assert_int_equal(0, out_len);

        // Case converting a boolean false
        if (snowflake_column_as_str(sfstmt, 4, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("", out);
        assert_int_equal(0, out_len);

        // Case converting a boolean true
        if (snowflake_column_as_str(sfstmt, 5, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("1", out);
        assert_int_equal(1, out_len);

        // Case with a date type
        if (snowflake_column_as_str(sfstmt, 6, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("2018-09-14", out);
        assert_int_equal(10, out_len);

        // Case with a TIMESTAMP_LTZ type
        if (snowflake_column_as_str(sfstmt, 7, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("2014-03-20 15:30:45.493679329", out);
        assert_int_equal(29, out_len);

        // Case with a TIMESTAMP_NTZ type
        if (snowflake_column_as_str(sfstmt, 8, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("2014-03-20 15:30:45.493679329", out);
        assert_int_equal(29, out_len);

        // Case with a TIMESTAMP_TZ type
        if (snowflake_column_as_str(sfstmt, 9, &out, &out_len, &out_max_size)) {
            dump_error(&(sfstmt->error));
        }
        assert_string_equal("2014-03-20 15:30:45.493679329 -07:00", out);
        assert_int_equal(36, out_len);

        // Out of bounds check
        if (!(status = snowflake_column_as_str(sfstmt, 10, &out, &out_len, &out_max_size))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);

        if (!(status = snowflake_column_as_str(sfstmt, -1, &out, &out_len, &out_max_size))) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_ERROR_OUT_OF_BOUNDS);
    }

    free(out);
    out = NULL;
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_column_as_boolean_arrow(void **unused) {
    test_column_as_boolean_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_boolean_json(void **unused) {
    test_column_as_boolean_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_int8_arrow(void **unused) {
    test_column_as_int8_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_int8_json(void **unused) {
    test_column_as_int8_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_int32_arrow(void **unused) {
    test_column_as_int32_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_int32_json(void **unused) {
    test_column_as_int32_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_int64_arrow(void **unused) {
    test_column_as_int64_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_int64_json(void **unused) {
    test_column_as_int64_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_uint8_arrow(void **unused) {
    test_column_as_uint8_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_uint8_json(void **unused) {
    test_column_as_uint8_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_uint32_arrow(void **unused) {
    test_column_as_uint32_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_uint32_json(void **unused) {
    test_column_as_uint32_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_uint64_arrow(void **unused) {
    test_column_as_uint64_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_uint64_json(void **unused) {
    test_column_as_uint64_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_float32_arrow(void **unused) {
    test_column_as_float32_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_float32_json(void **unused) {
    test_column_as_float32_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_float64_arrow(void **unused) {
    test_column_as_float64_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_float64_json(void **unused) {
    test_column_as_float64_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_timestamp_arrow(void **unused) {
    test_column_as_timestamp_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_timestamp_json(void **unused) {
    test_column_as_timestamp_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_timestamp_windows_arrow(void **unused) {
    test_column_as_timestamp_windows_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_timestamp_windows_json(void **unused) {
    test_column_as_timestamp_windows_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_const_str_arrow(void **unused) {
    test_column_as_const_str_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_const_str_json(void **unused) {
    test_column_as_const_str_helper(SF_BOOLEAN_FALSE);
}

void test_column_is_null_arrow(void **unused) {
    test_column_is_null_helper(SF_BOOLEAN_TRUE);
}

void test_column_is_null_json(void **unused) {
    test_column_is_null_helper(SF_BOOLEAN_FALSE);
}

void test_column_strlen_arrow(void **unused) {
    test_column_strlen_helper(SF_BOOLEAN_TRUE);
}

void test_column_strlen_json(void **unused) {
    test_column_strlen_helper(SF_BOOLEAN_FALSE);
}

void test_column_as_str_arrow(void **unused) {
    test_column_as_str_helper(SF_BOOLEAN_TRUE);
}

void test_column_as_str_json(void **unused) {
    test_column_as_str_helper(SF_BOOLEAN_FALSE);
}

sf_bool set_bundle_202408_enabled(sf_bool enabled)
{
  SF_CONNECT* sf = setup_snowflake_connection();

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS)
  {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_query(sfstmt, "use role accountadmin;", 0);
  if (status != SF_STATUS_SUCCESS)
  {
    dump_error(&(sfstmt->error));
    // can't use accoutadmin, return false to skip the test
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    return SF_BOOLEAN_FALSE;
  }

  if (enabled)
  {
    status = snowflake_query(sfstmt, "select SYSTEM$ENABLE_BEHAVIOR_CHANGE_BUNDLE('2024_08');", 0);
  }
  else
  {
    status = snowflake_query(sfstmt, "select SYSTEM$DISABLE_BEHAVIOR_CHANGE_BUNDLE('2024_08');", 0);
  }
  if (status != SF_STATUS_SUCCESS)
  {
    dump_error(&(sfstmt->error));
    // can't setup BCR bundle, return false to skip the test
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    return SF_BOOLEAN_FALSE;
  }

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);

  return SF_BOOLEAN_TRUE;
}

void test_column_length_with_bundle_202408_helper(sf_bool enabled)
{
  if (!set_bundle_202408_enabled(enabled))
  {
    sf_fprintf(stderr, "test_column_length_with_bundle_202408: failed to setup BCR bundle, skip.\n");
    return;
  }

  int i = 0;
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS)
  {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_query(sfstmt,
                           "SELECT PARSE_XML('<test>22257e111</test>'), TO_BINARY('1234'), "
                           "TO_VARCHAR('1234'), TO_DATE('1989-12-30'), TO_TIME('12:30'), "
                           "TO_TIMESTAMP_LTZ('1989-12-30 12:30'), TO_TIMESTAMP_NTZ('1989-12-30 12:30'), "
                           "TO_TIMESTAMP_TZ('1989-12-30 12:30'), TO_NUMBER('1234'), "
                           "TO_DOUBLE('1234'), TO_BOOLEAN(1), "
                           "TO_VARIANT('1234'), TO_ARRAY('1234'), TO_OBJECT(PARSE_JSON('{\"a\":1}'));", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  SF_COLUMN_DESC* desc = snowflake_desc(sfstmt);
  uint64* max_varchar_size_p = NULL;
  uint64* max_binary_size_p = NULL;
  uint64* max_variant_size_p = NULL;
  snowflake_get_attribute(sfstmt->connection, SF_CON_MAX_VARCHAR_SIZE, (void**)&max_varchar_size_p);
  snowflake_get_attribute(sfstmt->connection, SF_CON_MAX_BINARY_SIZE, (void**)&max_binary_size_p);
  snowflake_get_attribute(sfstmt->connection, SF_CON_MAX_VARIANT_SIZE, (void**)&max_variant_size_p);
  // PARSE_XML
  assert_int_equal(desc[0].c_type, SF_C_TYPE_STRING);
  assert_int_equal(desc[0].byte_size, *max_varchar_size_p);
  assert_int_equal(desc[0].internal_size, *max_varchar_size_p);
  // BINARY
  assert_int_equal(desc[1].c_type, SF_C_TYPE_BINARY);
  assert_int_equal(desc[1].byte_size, *max_binary_size_p);
  assert_int_equal(desc[1].internal_size, *max_binary_size_p);
  // VARCHAR
  assert_int_equal(desc[2].byte_size, *max_varchar_size_p);
  assert_int_equal(desc[2].internal_size, *max_varchar_size_p);
  // types with fixed length date, time, ltz, ntz, tz, number, double, boolean
  for (i = 3; i < 11; i++)
  {
    assert_int_equal(desc[i].byte_size, 0);
    assert_int_equal(desc[i].internal_size, 0);
  }
  // variant, object, array
  for (i = 11; i < 14; i++)
  {
      assert_int_equal(desc[i].byte_size, *max_variant_size_p);
      assert_int_equal(desc[i].internal_size, *max_variant_size_p);
  }
  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_column_length_with_bundle_202408_disabled(void** unused) {
  SF_UNUSED(unused);
  test_column_length_with_bundle_202408_helper(SF_BOOLEAN_FALSE);
}

void test_column_length_with_bundle_202408_enabled(void** unused) {
  SF_UNUSED(unused);
  test_column_length_with_bundle_202408_helper(SF_BOOLEAN_TRUE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_column_as_boolean_arrow),
      cmocka_unit_test(test_column_as_boolean_json),
      cmocka_unit_test(test_column_as_int8_arrow),
      cmocka_unit_test(test_column_as_int8_json),
      cmocka_unit_test(test_column_as_int32_arrow),
      cmocka_unit_test(test_column_as_int32_json),
      cmocka_unit_test(test_column_as_int64_arrow),
      cmocka_unit_test(test_column_as_int64_json),
      cmocka_unit_test(test_column_as_uint8_arrow),
      cmocka_unit_test(test_column_as_uint8_json),
      cmocka_unit_test(test_column_as_uint32_arrow),
      cmocka_unit_test(test_column_as_uint32_json),
      cmocka_unit_test(test_column_as_uint64_arrow),
      cmocka_unit_test(test_column_as_uint64_json),
      cmocka_unit_test(test_column_as_float32_arrow),
      cmocka_unit_test(test_column_as_float32_json),
      cmocka_unit_test(test_column_as_float64_arrow),
      cmocka_unit_test(test_column_as_float64_json),
#ifndef _WIN32
      cmocka_unit_test(test_column_as_timestamp_arrow),
      cmocka_unit_test(test_column_as_timestamp_json),
#else 
      cmocka_unit_test(test_column_as_timestamp_windows_arrow),
      cmocka_unit_test(test_column_as_timestamp_windows_json),
#endif
      cmocka_unit_test(test_column_as_const_str_arrow),
      cmocka_unit_test(test_column_as_const_str_json),
      cmocka_unit_test(test_column_is_null_arrow),
      cmocka_unit_test(test_column_is_null_json),
      cmocka_unit_test(test_column_strlen_arrow),
      cmocka_unit_test(test_column_strlen_json),
      cmocka_unit_test(test_column_as_str_arrow),
      cmocka_unit_test(test_column_as_str_json),
      cmocka_unit_test(test_column_length_with_bundle_202408_disabled),
      cmocka_unit_test(test_column_length_with_bundle_202408_enabled),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
