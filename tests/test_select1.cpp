/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

void test_select1_cpp(void **unused) {
    Snowflake::Client::Connection *cnx = TestSetup::connectionFactory();
    SF_STATUS status;

    try {
        cnx->connect();
        Snowflake::Client::Statement *stmt = new Snowflake::Client::Statement(*cnx);
        std::string command("select 1;");
        stmt->query(command);

        int64 out = 0;
        SF_BIND_OUTPUT c1 = {
          idx : 1,
          c_type : SF_C_TYPE_INT64,
          max_length : 0,
          value : (void *) &out,
          len : sizeof(out),
          is_null : 0
        };
        stmt->bindResult(c1);

        int counter = 0;
        while ((status = stmt->fetch()) == SF_STATUS_SUCCESS) {
            assert_int_equal(*(int64 *) c1.value, 1);
            ++counter;
        }
        //assert_int_equal(status, SF_STATUS_EOF);

        delete stmt;
        delete cnx;
    } catch (...) /* Catch all error handler */ {
        // Handle errors
    }
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_select1_cpp),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
