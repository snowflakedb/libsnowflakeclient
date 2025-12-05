#include <string.h>
#include <assert.h>
#include "utils/test_setup.h"
#include <limits.h>
#include <float.h>
#include <math.h>

typedef struct test_case_to_string {
    const int64 c1in;
    const float64 c2in;
    const int64 c3in;
    const float64 c4in;

    const char *c2out;
    const char *c3out;
    const char *c4out;
} TEST_CASE_TO_STRING;

typedef struct decfloat_testing_case {
    const int64 id;
    char c1in[100];
    char c2in[100];

    const char* c1strout;
    const char* c2strout;
} DECFLOAT_TESTING_CASE;

void test_number_helper(sf_bool use_arrow) {

    TEST_CASE_TO_STRING test_cases[] = {
      {.c1in = 1, .c2in = 123.456, .c3in = 98765, .c4in = 234.5678, .c2out="123.456000", .c3out="98765", .c4out="234.5678"},
      {.c1in = 2, .c2in = 12345678.987, .c3in = -12345678901234567, .c4in = -0.000123, .c2out="12345678.987000", .c3out="-12345678901234567", .c4out="-0.000123"}
    };

    SF_STATUS status;
    SF_CONNECT *sf = setup_snowflake_connection();

    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt,
                    use_arrow == SF_BOOLEAN_TRUE
                    ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
                    : "alter session set C_API_QUERY_RESULT_FORMAT=JSON",
                    0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(
      sfstmt,
      "create or replace table t (c1 int, c2 number(38,6), c3 number(18,0), c4 float)",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* insert data */
    status = snowflake_prepare(
      sfstmt,
      "insert into t(c1,c2,c3,c4) values(?,?,?,?)",
      0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    size_t i;
    size_t len;
    for (i = 0, len = sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING);
         i < len; i++) {
        TEST_CASE_TO_STRING v = test_cases[i];

        SF_BIND_INPUT ic1 = {0};
        ic1.idx = 1;
        ic1.name = NULL;
        ic1.c_type = SF_C_TYPE_INT64;
        ic1.value = (void *) &v.c1in;
        ic1.len = sizeof(v.c1in);
        status = snowflake_bind_param(sfstmt, &ic1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic2 = {0};
        ic2.idx = 2;
        ic2.name = NULL;
        ic2.c_type = SF_C_TYPE_FLOAT64;
        ic2.value = (void *) &v.c2in;
        ic2.len = sizeof(v.c2in);
        status = snowflake_bind_param(sfstmt, &ic2);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic3 = {0};
        ic3.idx = 3;
        ic3.name = NULL;
        ic3.c_type = SF_C_TYPE_INT64;
        ic3.value = (void *) &v.c3in;
        ic3.len = sizeof(v.c3in);
        status = snowflake_bind_param(sfstmt, &ic3);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic4 = {0};
        ic4.idx = 4;
        ic4.name = NULL;
        ic4.c_type = SF_C_TYPE_FLOAT64;
        ic4.value = (void *) &v.c4in;
        ic4.len = sizeof(v.c4in);
        status = snowflake_bind_param(sfstmt, &ic4);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        status = snowflake_execute(sfstmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
    }
    /* query */
    status = snowflake_query(sfstmt, "select * from t order by 1", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt),
                     sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING));

    int64 c1 = 0;
    char *str = NULL;
    size_t str_len = 0;
    size_t max_str_len = 0;
    int64 int_val = 0;
    float64 float_val = 0.0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &c1);
        TEST_CASE_TO_STRING v = test_cases[c1 - 1];
        snowflake_column_as_float64(sfstmt, 2, &float_val);
        snowflake_column_as_str(sfstmt, 2, &str, &str_len, &max_str_len);
        assert(float_val = v.c2in);
        assert_string_equal(v.c2out, str);
        snowflake_column_as_int64(sfstmt, 3, &int_val);
        snowflake_column_as_str(sfstmt, 3, &str, &str_len, &max_str_len);
        assert(int_val = v.c3in);
        assert_string_equal(v.c3out, str);
        snowflake_column_as_float64(sfstmt, 4, &float_val);
        snowflake_column_as_str(sfstmt, 4, &str, &str_len, &max_str_len);
        assert(float_val = v.c4in);
        assert_string_equal(v.c4out, str);
    }
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);

    status = snowflake_query(sfstmt, "drop table if exists t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    free(str);
    str = NULL;
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_decfloat_helper(sf_bool use_arrow) {

    DECFLOAT_TESTING_CASE test_cases[] = {
      {.id = 1, .c1in = "123.456",  .c1strout = "123.456", .c2in = "-345.456", .c2strout = "-345.456"},
      {.id = 2, .c1in = "123456789012345678901234567890123456.781234", .c1strout = "123456789012345678901234567890123456.78", .c2in = "123456789012345678901234567890123456785", .c2strout = "1.2345678901234567890123456789012345679e38"},
      {.id = 3, .c1in = "99999999999999999999999999999999999999", .c1strout = "99999999999999999999999999999999999999", .c2in = "999999999999999999999999999999999999999 ", .c2strout = "1e38"},
      {.id = 4, .c1in = "123456789012345678901234567890123456780000", .c1strout = "1.2345678901234567890123456789012345678e41", .c2in = "0.00000123456789012345678901", .c2strout = "0.00000123456789012345678901"},
      {.id = 5, .c1in = "0.0000000000000000000000000123456789012", .c1strout = "0.0000000000000000000000000123456789012",.c2in = "0.00000000000000000000000000000123456789012345678901", .c2strout = "1.23456789012345678901e-30"},
      {.id = 6, .c1in = "1e37", .c1strout = "10000000000000000000000000000000000000",.c2in = "1e38", .c2strout = "1e38"},
      {.id = 7, .c1in = "-1e-37", .c1strout = "-0.0000000000000000000000000000000000001",.c2in = "-1e-38", .c2strout = "-1e-38"},
    };

    SF_STATUS status;
    SF_CONNECT* sf = setup_snowflake_connection();

    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT* sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt,
        use_arrow == SF_BOOLEAN_TRUE
        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(
        sfstmt,
        "create or replace table decfloat_testing (id int, c1 decfloat, c2 decfloat)",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* insert data */
    status = snowflake_prepare(
        sfstmt,
        "insert into decfloat_testing(id, c1,c2) values(?,?,?)",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    size_t i;
    size_t len;
    for (i = 0, len = sizeof(test_cases) / sizeof(DECFLOAT_TESTING_CASE);
        i < len; i++) {
        DECFLOAT_TESTING_CASE v = test_cases[i];

        SF_BIND_INPUT ic1 = { 0 };
        ic1.idx = 1;
        ic1.name = NULL;
        ic1.c_type = SF_C_TYPE_INT64;
        ic1.value = (void*)&v.id;
        ic1.len = sizeof(v.id);
        status = snowflake_bind_param(sfstmt, &ic1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic2 = { 0 };
        ic2.idx = 2;
        ic2.name = NULL;
        ic2.c_type = SF_C_TYPE_STRING;
        ic2.value = v.c1in;
        ic2.len = sizeof(v.c1in);
        status = snowflake_bind_param(sfstmt, &ic2);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic3 = { 0 };
        ic3.idx = 3;
        ic3.name = NULL;
        ic3.c_type = SF_C_TYPE_STRING;
        ic3.value = v.c2in;
        ic3.len = sizeof(v.c2in);
        status = snowflake_bind_param(sfstmt, &ic3);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        status = snowflake_execute(sfstmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
    }
    /* query */
    status = snowflake_query(sfstmt, "select * from decfloat_testing order by id", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt),
        sizeof(test_cases) / sizeof(DECFLOAT_TESTING_CASE));

    int64 c1 = 0;
    char* str = NULL;
    size_t str_len = 0;
    size_t max_str_len = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &c1);
        DECFLOAT_TESTING_CASE v = test_cases[c1 - 1];
        snowflake_column_as_str(sfstmt, 2, &str, &str_len, &max_str_len);
        assert_string_equal(v.c1strout, str);
    }

    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);

    status = snowflake_query(sfstmt, "drop table if exists decfloat_testing", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    free(str);
    str = NULL;
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_numeric_range_helper(sf_bool use_arrow) {
    typedef struct numeric_case {
        int64 id;
        char val[256];

        SF_STATUS exp_i32_status;
        int32 exp_i32;

        SF_STATUS exp_i64_status;
        int64 exp_i64;

        SF_STATUS exp_u32_status;
        uint32 exp_u32;

        SF_STATUS exp_u64_status;
        uint64 exp_u64;

        SF_STATUS exp_f32_status;
        float32 exp_f32;

        SF_STATUS exp_f64_status;
        float64 exp_f64;
    } NUMERIC_CASE;

    NUMERIC_CASE cases[] = {
        ///////* simple small integer */
        {.id = 1, .val = "123.456",
          .exp_i32_status = SF_STATUS_SUCCESS, .exp_i32 = 123,
          .exp_i64_status = SF_STATUS_SUCCESS, .exp_i64 = 123,
          .exp_u32_status = SF_STATUS_SUCCESS, .exp_u32 = 123,
          .exp_u64_status = SF_STATUS_SUCCESS, .exp_u64 = 123,
          .exp_f32_status = SF_STATUS_SUCCESS, .exp_f32 = 123.456f,
          .exp_f64_status = SF_STATUS_SUCCESS, .exp_f64 = 123.456
        },
        {.id = 2, .val = "2e10",
          .exp_i32_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_i64_status = SF_STATUS_SUCCESS, .exp_i64 = 20000000000,
          .exp_u32_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_u64_status = SF_STATUS_SUCCESS, .exp_u64 = 20000000000ULL,
          .exp_f32_status = SF_STATUS_SUCCESS, .exp_f32 = 20000000000.0f,
          .exp_f64_status = SF_STATUS_SUCCESS, .exp_f64 = 20000000000.0
        },
        ////Where the scientific notion starts in db
        {.id = 3, .val = "123e36",
          .exp_i32_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_i64_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_u32_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_u64_status = SF_STATUS_ERROR_OUT_OF_RANGE,

          //The result is from std::stof and std::stod conversion. May lose precision and some values.
          .exp_f32_status = SF_STATUS_SUCCESS, .exp_f32 = 123e36f,
          .exp_f64_status = SF_STATUS_SUCCESS, .exp_f64 = 123e36
        },
        {.id = 4, .val = "123e100",
          .exp_i32_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_i64_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_u32_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_u64_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_f32_status = SF_STATUS_ERROR_OUT_OF_RANGE,
          .exp_f64_status = SF_STATUS_SUCCESS, .exp_f64 = 1.23e+102 
        },
        {.id = 5, .val = "1e-38",
          .exp_i32_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_i64_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_u32_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_u64_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_f32_status = SF_STATUS_SUCCESS, .exp_f32 = 1e-38f,
          .exp_f64_status = SF_STATUS_SUCCESS, .exp_f64 = 1e-38
        },
        {.id = 6, .val = "23456e-100",
          .exp_i32_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_i64_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_u32_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_u64_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_f32_status = SF_STATUS_ERROR_CONVERSION_FAILURE,
          .exp_f64_status = SF_STATUS_SUCCESS, .exp_f64 = 23456e-100
        },
    };

    SF_STATUS status;
    SF_CONNECT* sf = setup_snowflake_connection();

    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement and set qrf */
    SF_STMT* sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt,
        use_arrow == SF_BOOLEAN_TRUE
        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create table holding string representations */
    status = snowflake_query(
        sfstmt,
        "create or replace table decfloat_range (id int, c1 decfloat)",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Prepare insert */
    status = snowflake_prepare(sfstmt, "insert into decfloat_range(id, c1) values(?,?)", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Bind and insert cases */
    size_t i;
    size_t len = sizeof(cases) / sizeof(NUMERIC_CASE);
    for (i = 0; i < len; ++i) {
        NUMERIC_CASE v = cases[i];

        SF_BIND_INPUT ic1 = { 0 };
        ic1.idx = 1;
        ic1.name = NULL;
        ic1.c_type = SF_C_TYPE_INT64;
        ic1.value = (void*)&v.id;
        ic1.len = sizeof(v.id);
        status = snowflake_bind_param(sfstmt, &ic1);
        if (status != SF_STATUS_SUCCESS) { dump_error(&(sfstmt->error)); }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic2 = { 0 };
        ic2.idx = 2;
        ic2.name = NULL;
        ic2.c_type = SF_C_TYPE_STRING;
        ic2.value = v.val;
        ic2.len = sizeof(v.val);
        status = snowflake_bind_param(sfstmt, &ic2);
        if (status != SF_STATUS_SUCCESS) { dump_error(&(sfstmt->error)); }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        status = snowflake_execute(sfstmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
    }

    /* Query back */
    status = snowflake_query(sfstmt, "select * from decfloat_range order by id", 0);
    if (status != SF_STATUS_SUCCESS) { dump_error(&(sfstmt->error)); }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), sizeof(cases) / sizeof(NUMERIC_CASE));

    char* str = NULL;
    size_t str_len = 0;
    size_t max_str_len = 0;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        int64 rid = 0;
        status = snowflake_column_as_int64(sfstmt, 1, &rid);
        assert_int_equal(status, SF_STATUS_SUCCESS);
        NUMERIC_CASE expected = cases[rid - 1];

        /* int32 */
        int32 got_i32 = 0;
        status = snowflake_column_as_int32(sfstmt, 2, &got_i32);
        assert_int_equal(status, expected.exp_i32_status);
        if (status == SF_STATUS_SUCCESS) {
            assert_true(got_i32 == expected.exp_i32);
        }

        /* int64 */
        int64 got_i64 = 0;
        status = snowflake_column_as_int64(sfstmt, 2, &got_i64);
        assert_int_equal(status, expected.exp_i64_status);
        if (status == SF_STATUS_SUCCESS) {
            assert_true(got_i64 == expected.exp_i64);
        }

        /* uint32 */
        uint32 got_u32 = 0;
        status = snowflake_column_as_uint32(sfstmt, 2, &got_u32);
        assert_int_equal(status, expected.exp_u32_status);
        if (status == SF_STATUS_SUCCESS) {
            assert_true(got_u32 == expected.exp_u32);
        }

        /* uint64 */
        uint64 got_u64 = 0;
        status = snowflake_column_as_uint64(sfstmt, 2, &got_u64);
        assert_int_equal(status, expected.exp_u64_status);
        if (status == SF_STATUS_SUCCESS) {
            assert_true(got_u64 == expected.exp_u64);
        }

        /* float32 from fltstr (column 3) */
        float32 got_f32 = 0.0f;
        status = snowflake_column_as_float32(sfstmt, 2, &got_f32);
        assert_int_equal(status, expected.exp_f32_status);
        if (status == SF_STATUS_SUCCESS) {
            // the value may be lost precision during conversion, so use relative tolerance
            float32 tol = fmaxf(1e-6f, fabsf(expected.exp_f32) * 1e-6f);
            assert_true(fabsf(got_f32 - expected.exp_f32) <= tol);

        }

        /* float64 */
        float64 got_f64 = 0.0;
        status = snowflake_column_as_float64(sfstmt, 2, &got_f64);
        assert_int_equal(status, expected.exp_f64_status);
        if (status == SF_STATUS_SUCCESS) {
            double tol = fmax(1e-12, fabs(expected.exp_f64) * 1e-12);
            assert_true(fabs(got_f64 - expected.exp_f64) <= tol);

        }
    }

    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);

    status = snowflake_query(sfstmt, "drop table if exists t_range", 0);
    if (status != SF_STATUS_SUCCESS) { dump_error(&(sfstmt->error)); }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_number_arrow(void **unused) {
    SF_UNUSED(unused);
    test_number_helper(SF_BOOLEAN_TRUE);
}

void test_number_json(void **unused) {
    SF_UNUSED(unused);
    test_number_helper(SF_BOOLEAN_FALSE);
}

void test_decfloat_arrow(void **unused) {
    SF_UNUSED(unused);
    test_decfloat_helper(SF_BOOLEAN_TRUE);
}

void test_decfloat_json(void **unused) {
    SF_UNUSED(unused);
    test_decfloat_helper(SF_BOOLEAN_FALSE);
}

void test_numeric_range_arrow(void** unused) {
    SF_UNUSED(unused);
    test_numeric_range_helper(SF_BOOLEAN_TRUE);
}
void test_numeric_range_json(void** unused) {
    SF_UNUSED(unused);
    test_numeric_range_helper(SF_BOOLEAN_FALSE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_number_arrow),
      cmocka_unit_test(test_number_json),
      cmocka_unit_test(test_decfloat_arrow),
      cmocka_unit_test(test_decfloat_json),
      cmocka_unit_test(test_numeric_range_arrow),
      cmocka_unit_test(test_numeric_range_json),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
