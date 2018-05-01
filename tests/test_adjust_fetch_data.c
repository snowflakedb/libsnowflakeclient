/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

/**
 * Test SF_STMT_USER_REALLOC_FUNC, which reallocates a larger memory
 * in case the given memory size is not large enough.
 * @param unused
 */
void test_select_long_data_with_small_initial_buffer(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT *sfstmt = snowflake_stmt(sf);

    // realloc function is used to allocate a new memory when the given buffer
    // size is not large enough to fit.
    snowflake_stmt_set_attr(sfstmt, SF_STMT_USER_REALLOC_FUNC, realloc);

    status = snowflake_query(sfstmt, "select randstr(100,random()) from table(generator(rowcount=>2))", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    size_t init_size = 10;
    SF_BIND_OUTPUT c1 = {
        .idx = 1,
        .c_type = SF_C_TYPE_STRING,
        .value = calloc(1, init_size),
        .len = init_size,
        .max_length = init_size
    };
    snowflake_bind_result(sfstmt, &c1);
    assert_int_equal(snowflake_num_rows(sfstmt), 2);

    int counter = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        assert_int_equal(strlen(c1.value), 100);
        ++counter;
    }
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);
    free(c1.value);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_select_long_data_with_small_initial_buffer),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
