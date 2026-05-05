#include "../wiremock/wiremock.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include "snowflake/client.h"
#include "snowflake/secure_storage.h"
#include "../utils/test_setup.h"
#include "../utils/TestSetup.hpp"
#include "snowflake/QueryContextCache.hpp"

using namespace Snowflake::Client;

WiremockRunner* wiremock = NULL;

int setup_wiremock(void **) {
  snowflake_global_set_attribute(SF_GLOBAL_DISABLE_VERIFY_PEER, &SF_BOOLEAN_TRUE);
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_FALSE);
  wiremock = new WiremockRunner();

  return 0;
}

int teardown_wiremock(void **) {
  if (wiremock) {
    delete wiremock;
    wiremock = nullptr;
  }
  return 0;
}

void test_handle_qcc_on_failure_query(void** unused)
{
    SF_UNUSED(unused);
    wiremock->resetMapping();
    wiremock->initMappingFromMultiFile("login_response.json",{
                                      "table_creation.json",
                                      "first_row_insertion.json",
                                      "duplicated_data_insertion.json",
                                      });


    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, wiremockHost);
    snowflake_set_attribute(sf, SF_CON_PORT, wiremockPort);
    snowflake_set_attribute(sf, SF_CON_TIMEZONE, "UTC");

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    QueryContextCache* qcc = static_cast<QueryContextCache*>(sf->qcc);
    SF_STMT* sfstmt = NULL;

    /* execute a DML */
    sfstmt = snowflake_stmt(sf);
    
    status = snowflake_query(
        sfstmt,
        "CREATE OR REPLACE HYBRID TABLE tkv (k NUMBER PRIMARY KEY, v VARCHAR);",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(qcc->getSize(), 1);

    status = snowflake_query(
        sfstmt,
        "INSERT INTO tkv VALUES (2, 'valuetesting');",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(qcc->getSize(), 2);

    status = snowflake_query(
        sfstmt,
        "INSERT INTO tkv VALUES (2, 'valuetesting');",
        0
    );
    //Should be failed because of the same primary key.
    if (status == SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(qcc->getSize(), 2);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
   initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_handle_qcc_on_failure_query),
  };
  int ret = cmocka_run_group_tests(tests, setup_wiremock, teardown_wiremock);
  snowflake_global_term();
  return ret;
}
