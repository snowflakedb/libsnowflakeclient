#include <string>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "snowflake/QueryContextCache.hpp"

using namespace Snowflake::Client;

void test_handle_qcc_on_failure_query(void** unused)
{
    SF_STATUS status;
    SF_CONNECT* sf = setup_snowflake_connection();
    int64 timeout = 1200;
    snowflake_set_attribute(sf, SF_CON_NETWORK_TIMEOUT, &timeout);
    snowflake_set_attribute(sf, SF_CON_RETRY_TIMEOUT, &timeout);
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    QueryContextCache* qcc = static_cast<QueryContextCache*>(sf->qcc);

    SF_STMT* sfstmt = NULL;

    /* execute a DML */
    sfstmt = snowflake_stmt(sf);
    /*
    status = snowflake_query(
        sfstmt,
        "CREATE OR REPLACE HYBRID TABLE tkv (k NUMBER PRIMARY KEY, v VARCHAR);",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(qcc->getSize(), 1);*/

    status = snowflake_query(
        sfstmt,
        "ALTER SESSION SET TRANSACTION_ABORT_ON_ERROR = true",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(qcc->getSize(), 1);

    status = snowflake_query(
        sfstmt,
        "insert into tkv values (2, 'valuetesting');",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(qcc->getSize(), 2);

    status = snowflake_query(
        sfstmt,
        "insert into tkv values (2, 'valuetesting');",
        0
    );
    //Should be failed because of the same primary key.
    if (status == SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(qcc->getSize(), 2);

    status = snowflake_query(
        sfstmt,
        "SELECT * FROM tkv;",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(qcc->getSize(), 2);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf); // purge snowflake context

}

int main(void) {
   initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_handle_qcc_on_failure_query),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
