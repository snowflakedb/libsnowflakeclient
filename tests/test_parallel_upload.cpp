/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <aws/core/Aws.h>
#include <snowflake/client.h>
#include <fstream>
#include "utils/test_setup.h"
#include "IStatementPutGet.hpp"
#include "StatementPutGet.hpp"
#include "FileTransferAgent.hpp"

#define SMALL_FILE_PREFIX "test_small_file_"
#define LARGE_FILE_NAME "test_large_file.csv"

void getDataDirectory(std::string& dataDir)
{
  const std::string current_file = __FILE__;
  std::string testsDir = current_file.substr(0, current_file.find_last_of('/'));
  dataDir = testsDir + "/data/";
}

void populateDataInTestDir(std::string &testDir, int numberOfFiles)
{
  std::string mkdirCmd = "mkdir -p " + testDir;
  int ret = system(mkdirCmd.c_str());
  assert_int_equal(0, ret);

  if (numberOfFiles == 1)
  {
    std::string fullFileName = testDir + LARGE_FILE_NAME;
    std::ofstream ofs(fullFileName);
    assert_true(ofs.is_open());

    for (int i=0; i<500000; i++)
    {
      ofs << "test_string11111,test_string222222,test_string333333" << std::endl;
    }
    ofs.close();
  }
  else
  {
    for (int i=0; i<numberOfFiles;i ++)
    {
      std::string fullFileName = testDir + SMALL_FILE_PREFIX +
                                 std::to_string(i) + ".csv";
      std::ofstream ofs(fullFileName);
      assert_true(ofs.is_open());
      ofs << "1,2,test_string" << std::endl;
      ofs.close();
    }

  }
}

void test_parallel_upload_core(int fileNumber)
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

  std::string create_table("create or replace table test_parallel_upload(c1 string"
                             ", c2 string, c3 string)");
  ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  std::string dataDir;
  getDataDirectory(dataDir);
  std::string testDir = dataDir + "test_parallel_upload/";

  populateDataInTestDir(testDir, fileNumber);

  std::string files = testDir + "*";
  std::string putCommand = "put file://" + files + " @%test_parallel_upload auto_compress=false";

  std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
    <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

  FileTransferExecutionResult *result = agent.execute(&putCommand);
  assert_int_equal(fileNumber, result->getResultSize());

  while(result->next())
  {
    assert_string_equal("SUCCEED", result->getStatus());
  }

  std::string copyCommand = "copy into test_parallel_upload";
  ret = snowflake_query(sfstmt, copyCommand.c_str(), copyCommand.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  char out_c2[7];
  SF_BIND_OUTPUT c2;
  c2.idx = 2;
  c2.c_type = SF_C_TYPE_STRING;
  c2.max_length = sizeof(out_c2);
  c2.value = (void *) out_c2;
  c2.len = sizeof(out_c2);

  ret = snowflake_bind_result(sfstmt, &c2);
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  assert_int_equal(snowflake_num_rows(sfstmt), fileNumber);

  while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
    assert_string_equal((char *) c2.value, "LOADED");
  }
  assert_int_equal(status, SF_STATUS_EOF);
  snowflake_stmt_term(sfstmt);

  /* close and term */
  snowflake_term(sf); // purge snowflake context
}

static int teardown(void **unused)
{
  SF_CONNECT *sf = setup_snowflake_connection();
  snowflake_connect(sf);

  SF_STMT *sfstmt = snowflake_stmt(sf);
  std::string rm = "rm @%test_parallel_upload";
  snowflake_query(sfstmt, rm.c_str(), rm.size());

  std::string truncate = "drop table if exists test_parallel_upload";
  snowflake_query(sfstmt, truncate.c_str(), truncate.size());

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);

  std::string dataDir;
  getDataDirectory(dataDir);
  std::string testDir = dataDir + "test_parallel_upload/";
  std::string rmCmd = "rm -rf " + testDir;
  system(rmCmd.c_str());
  return 0;
}

void test_small_file_concurrent_upload(void **unused)
{
  test_parallel_upload_core(10);
}

void test_large_file_multipart_upload(void **unused)
{
  test_parallel_upload_core(1);
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

static int gr_teardown(void **unused)
{
  snowflake_global_term();
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test_teardown(test_small_file_concurrent_upload, teardown),
    //cmocka_unit_test_teardown(test_large_file_multipart_upload, teardown),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
  return ret;
}
