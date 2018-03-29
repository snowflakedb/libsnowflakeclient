//
// Created by hyu on 3/20/18.
//

#include <aws/core/Aws.h>
#include <snowflake/client.h>
#include "utils/test_setup.h"
#include "IStatementPutGet.hpp"
#include "StatementPutGet.hpp"
#include "FileTransferAgent.hpp"

void getDataDirectory(std::string& dataDir)
{
  const std::string current_file = __FILE__;
  std::string testsDir = current_file.substr(0, current_file.find_last_of('/'));
  dataDir = testsDir + "/data/";
}

void test_simple_put(void** unused)
{
  /* init */
  SF_STATUS status;
  SF_CONNECT *sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    fprintf(stderr, "Connecting to snowflake failed, exiting...\n");
    SF_ERROR_STRUCT *error = snowflake_error(sf);
    fprintf(stderr, "Error message: %s\nIn File, %s, Line, %d\n",
            error->msg, error->file, error->line);

    snowflake_term(sf); // purge snowflake context
    snowflake_global_term();
    exit (1);
  }

  SF_STMT *sfstmt = NULL;
  SF_STATUS ret;

  /* query */
  sfstmt = snowflake_stmt(sf);

  std::string create_table("create or replace table test_small_put(c1 number"
                             ", c2 number, c3 string)");
  ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  std::string dataDir;
  getDataDirectory(dataDir);
  std::string file = dataDir + "small_file.csv";
  std::string putCommand = "put file://" + file + " @%test_small_put";

  std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
    <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

  agent.execute(&putCommand);
  std::vector<FileTransferExecutionResult *> * results = agent.getResult();
  assert_int_equal(1, results->size());

  FileTransferExecutionResult * result = results->at(0);
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  assert_string_equal("SUCCEED", result->status.c_str());

  std::string copyCommand = "copy into test_small_put from @%test_small_put";
  ret = snowflake_query(sfstmt, copyCommand.c_str(), copyCommand.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  std::string selectCommand = "select * from test_small_put";
  ret = snowflake_query(sfstmt, selectCommand.c_str(), selectCommand.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  char out_c1[5];
  SF_BIND_OUTPUT c1;
  c1.idx = 1;
  c1.c_type = SF_C_TYPE_STRING;
  c1.max_length = sizeof(out_c1);
  c1.value = (void *) out_c1;
  c1.len = sizeof(out_c1);

  char out_c2[5];
  SF_BIND_OUTPUT c2;
  c2.idx = 2;
  c2.c_type = SF_C_TYPE_STRING;
  c2.max_length = sizeof(out_c2);
  c2.value = (void *) out_c2;
  c2.len = sizeof(out_c2);

  char out_c3[20];
  SF_BIND_OUTPUT c3;
  c3.idx = 3;
  c3.c_type = SF_C_TYPE_STRING;
  c3.max_length = sizeof(out_c3);
  c3.value = (void *) out_c3;
  c3.len = sizeof(out_c3);

  ret = snowflake_bind_result(sfstmt, &c1);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  ret = snowflake_bind_result(sfstmt, &c2);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  ret = snowflake_bind_result(sfstmt, &c3);
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  assert_int_equal(snowflake_num_rows(sfstmt), 1);

  ret = snowflake_fetch(sfstmt);
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  assert_string_equal((char *)c1.value, "1");
  assert_string_equal((char *)c2.value, "2");
  assert_string_equal((char *)c3.value, "test_string");

  ret = snowflake_fetch(sfstmt);
  assert_int_equal(SF_STATUS_EOF, ret);

  snowflake_stmt_term(sfstmt);

  /* close and term */
  snowflake_term(sf); // purge snowflake context
}

static int teardown(void **unused)
{
  SF_CONNECT *sf = setup_snowflake_connection();
  snowflake_connect(sf);

  SF_STMT *sfstmt = snowflake_stmt(sf);
  std::string rm = "rm @%test_small_put";
  snowflake_query(sfstmt, rm.c_str(), rm.size());

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
  snowflake_global_term();
  return 0;
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test_setup_teardown(test_simple_put, NULL, teardown),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
