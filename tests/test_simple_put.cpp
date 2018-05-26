/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <aws/core/Aws.h>
#include <snowflake/client.h>
#include "utils/test_setup.h"
#include "snowflake/IStatementPutGet.hpp"
#include "StatementPutGet.hpp"
#include "FileTransferAgent.hpp"

void getDataDirectory(std::string& dataDir)
{
  const std::string current_file = __FILE__;
  std::string testsDir = current_file.substr(0, current_file.find_last_of('/'));
  dataDir = testsDir + "/data/";
}

void test_simple_put_core(const char * fileName,
                          const char * sourceCompression,
                          bool autoCompress,
                          bool copyUploadFile=true)
{
  /* init */
  SF_STATUS status;
  SF_CONNECT *sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  assert_int_equal(SF_STATUS_SUCCESS, status);

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
  std::string file = dataDir + fileName;
  std::string putCommand = "put file://" + file + " @%test_small_put";
  if (!autoCompress)
  {
    putCommand += " auto_compress=false";
  }

  putCommand += " source_compression=";
  putCommand += sourceCompression;

  std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
    <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

  ITransferResult * results = agent.execute(&putCommand);
  assert_int_equal(1, results->getResultSize());

  results->next();
  assert_string_equal("SUCCEED", results->getStatus());

  if (copyUploadFile)
  {
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

    assert_string_equal((char *) c1.value, "1");
    assert_string_equal((char *) c2.value, "2");
    assert_string_equal((char *) c3.value, "test_string");

    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_EOF, ret);
  }

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

  std::string truncate = "truncate table test_small_put";
  snowflake_query(sfstmt, truncate.c_str(), truncate.size());

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
  return 0;
}

void test_simple_put_auto_compress(void **unused)
{
  test_simple_put_core("small_file.csv", // filename
                       "auto", //source compression
                       true // auto compress
  );
}

void test_simple_put_auto_detect_gzip(void ** unused)
{
  test_simple_put_core("small_file.csv.gz", "auto", true);
}

void test_simple_put_no_compress(void **unused)
{
  test_simple_put_core("small_file.csv", "none", false);
}

void test_simple_put_gzip(void **unused)
{
  test_simple_put_core("small_file.csv.gz", "gzip", true);
}

void test_simple_put_zero_byte(void **unused)
{
  test_simple_put_core("zero_byte.csv", "auto", true, false);
}

void test_simple_put_one_byte(void **unused)
{
  test_simple_put_core("one_byte.csv", "auto", true, false);
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_TRUE);
  return 0;
}

static int gr_teardown(void **unused)
{
  snowflake_global_term();
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test_teardown(test_simple_put_auto_compress, teardown),
    cmocka_unit_test_teardown(test_simple_put_auto_detect_gzip, teardown),
    cmocka_unit_test_teardown(test_simple_put_no_compress, teardown),
    cmocka_unit_test_teardown(test_simple_put_gzip, teardown),
    cmocka_unit_test_teardown(test_simple_put_zero_byte, teardown),
    cmocka_unit_test_teardown(test_simple_put_one_byte, teardown),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
  return ret;
}
