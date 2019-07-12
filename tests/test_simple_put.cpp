/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <aws/core/Aws.h>
#include <snowflake/client.h>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "snowflake/IStatementPutGet.hpp"
#include "StatementPutGet.hpp"
#include "FileTransferAgent.hpp"

#define COLUMN_STATUS "STATUS"
#define COLUMN_SOURCE "SOURCE"
#define COLUMN_TARGET "TARGET"
#define COLUMN_SOURCE_COMPRESSION "SOURCE_COMPRESSION"
#define COLUMN_TARGET_COMPRESSION "TARGET_COMPRESSION"
#define COLUMN_ENCRYPTION "encryption"

using namespace ::Snowflake::Client;

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

  std::string dataDir = TestSetup::getDataDir();
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

  while(results->next())
  {
    std::string value;
    results->getColumnAsString(0, value); // source
    assert_string_equal(fileName, value.c_str());

    std::string expectedTarget = (autoCompress && !strstr(fileName, ".gz")) ?
                                 std::string(fileName) + ".gz" :
                                 std::string(fileName);
    results->getColumnAsString(1, value); // get target
    assert_string_equal(expectedTarget.c_str(), value.c_str());

    std::string expectedSourceCompression = !strstr(fileName, ".gz") ?
                                            "none" : "gzip";
    results->getColumnAsString(4, value); // get source_compression
    assert_string_equal(expectedSourceCompression.c_str(), value.c_str());

    std::string expectedTargetCompression = (!autoCompress &&
      !strstr(fileName, ".gz")) ? "none" : "gzip";
    results->getColumnAsString(5, value); // get target_compression
    assert_string_equal(expectedTargetCompression.c_str(), value.c_str());

    results->getColumnAsString(6, value); // get encryption
    assert_string_equal("UPLOADED", value.c_str());

    results->getColumnAsString(7, value); // get encryption
    assert_string_equal("ENCRYPTED", value.c_str());
  }

  if (copyUploadFile)
  {
    std::string copyCommand = "copy into test_small_put from @%test_small_put";
    ret = snowflake_query(sfstmt, copyCommand.c_str(), copyCommand.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    std::string selectCommand = "select * from test_small_put";
    ret = snowflake_query(sfstmt, selectCommand.c_str(), selectCommand.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    const char *out_c1;
    const char *out_c2;
    const char *out_c3;
    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    
    snowflake_column_as_const_str(sfstmt, 1, &out_c1);
    snowflake_column_as_const_str(sfstmt, 2, &out_c2);
    snowflake_column_as_const_str(sfstmt, 3, &out_c3);

    assert_string_equal(out_c1, "1");
    assert_string_equal(out_c2, "2");
    assert_string_equal(out_c3, "test_string");

    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_EOF, ret);
  }

  snowflake_stmt_term(sfstmt);

  /* close and term */
  snowflake_term(sf); // purge snowflake context
}

static int donothing(void **unused)
{
    return 0;
}

static int teardown(void **unused)
{
  SF_CONNECT *sf = setup_snowflake_connection();
  snowflake_connect(sf);

  SF_STMT *sfstmt = snowflake_stmt(sf);
  std::string truncate = "drop table if exists test_small_put";
  snowflake_query(sfstmt, truncate.c_str(), truncate.size());

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
  return 0;
}

void test_simple_get_data(const char *getCommand, const char *size)
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

    std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
            <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));

    Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

    // load first time should return uploaded
    std::string get_status;
    std::string getcmd(getCommand);
    ITransferResult * results = agent.execute(&getcmd);
    while(results && results->next())
    {
        results->getColumnAsString(1, get_status);
        //assert_string_equal(size, get_status.c_str()); //Compressed File sizes vary on Windows/Linux.
        results->getColumnAsString(2, get_status);
        assert_string_equal("DOWNLOADED", get_status.c_str());
        results->getColumnAsString(3, get_status);
        assert_string_equal("DECRYPTED", get_status.c_str());
    }

    snowflake_stmt_term(sfstmt);

    /* close and term */
    snowflake_term(sf); // purge snowflake context

}

void test_large_put_auto_compress(void **unused)
{
  std::string destinationfile="large_file.csv";
  std::string destFile = TestSetup::getDataDir() + destinationfile;
  FILE *fp =fopen(destFile.c_str(),"w");
  char str[]="abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
  for(int i=0;i<2000000;i++){
      fprintf(fp, "%d,%s\n",i,str);
  }
  fclose(fp);
  test_simple_put_core(destinationfile.c_str(), // filename
                       "auto", //source compression
                       true, // auto compress
                       false // DO NOT Load data into table
  );
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

void test_simple_get(void **unused)
{
  char tempDir[MAX_PATH] = { 0 };
  char tempPath[MAX_PATH + 256] ="get @%test_small_put/small_file.csv.gz file://";
  sf_get_tmp_dir(tempDir);
  strcat(tempPath, tempDir);
  test_simple_get_data(tempPath, "48");
}

void test_large_get(void **unused)
{
  char tempDir[MAX_PATH] = { 0 };
  char tempPath[MAX_PATH + 256] = "get @%test_small_put/large_file.csv.gz file://";
  sf_get_tmp_dir(tempDir);
  strcat(tempPath, tempDir);
  test_simple_get_data(tempPath, "5166848");
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

void test_simple_put_skip(void **unused)
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

  std::string dataDir = TestSetup::getDataDir();
  std::string file = dataDir + "small_file.csv";
  std::string putCommand = "put file://" + file + " @%test_small_put";

  std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
    <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

  // load first time should return uploaded
  std::string put_status;
  ITransferResult * results = agent.execute(&putCommand);
  while(results->next())
  {
    results->getColumnAsString(6, put_status);
    assert_string_equal("UPLOADED", put_status.c_str());
  }

  // load second time should return skipped
  results = agent.execute(&putCommand);
  while(results->next())
  {
    results->getColumnAsString(6, put_status);
    assert_string_equal("SKIPPED", put_status.c_str());
  }

  snowflake_stmt_term(sfstmt);

  /* close and term */
  snowflake_term(sf); // purge snowflake context

}


int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test_teardown(test_simple_put_auto_compress, teardown),
    cmocka_unit_test_teardown(test_simple_put_auto_detect_gzip, teardown),
    cmocka_unit_test_teardown(test_simple_put_no_compress, teardown),
    cmocka_unit_test_teardown(test_simple_put_gzip, teardown),
    cmocka_unit_test_teardown(test_simple_put_zero_byte, teardown),
    cmocka_unit_test_teardown(test_simple_put_one_byte, teardown),
    cmocka_unit_test_teardown(test_simple_put_skip, teardown),
///#ifdef PUT_GET_LARGE_DATASET_TEST
    cmocka_unit_test_teardown(test_simple_put_skip, donothing),
    cmocka_unit_test_teardown(test_simple_get, teardown),
    cmocka_unit_test_teardown(test_large_put_auto_compress, donothing),
    cmocka_unit_test_teardown(test_large_get, teardown)
///#endif
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
  return ret;
}
