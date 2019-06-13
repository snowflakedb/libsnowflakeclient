/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
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

        int counter = 0;
        while ((status = stmt->fetch()) == SF_STATUS_SUCCESS) {
            assert_int_equal(stmt->column(1).asInt32(), 1);
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
