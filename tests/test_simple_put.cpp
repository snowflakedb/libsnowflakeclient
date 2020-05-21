/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <aws/core/Aws.h>
#include <vector>
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

//File list to be made available to re-upload.
static std::vector<std::string> fileList;
static bool createOnlyOnce = true;

void replaceStrAll(std::string& stringToReplace,
                std::string const& oldValue,
                std::string const& newValue)
{
  size_t oldValueLen = oldValue.length();
  size_t newValueLen = newValue.length();
  if (0 == oldValueLen)
  {
    return;
  }

  size_t index = 0;
  while (true) {
    /* Locate the substring to replace. */
    index = stringToReplace.find(oldValue, index);
    if (index == std::string::npos) break;

    /* Make the replacement. */
    stringToReplace.replace(index, oldValueLen, newValue);

    /* Advance index forward so the next iteration doesn't pick it up as well. */
    index += newValueLen;
  }
}

void test_simple_put_core(const char * fileName,
                          const char * sourceCompression,
                          bool autoCompress,
                          bool copyUploadFile=true,
                          bool verifyCopyUploadFile=true,
                          bool copyTableToStaging=false,
                          bool createDupTable=false,
                          bool setCustomThreshold=false,
                          size_t customThreshold=64*1024*1024)
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

  std::string create_table;
  if(createDupTable && createOnlyOnce) {
      std::string create_table_temp("create or replace table test_small_put_dup(c1 number"
                                    ", c2 number, c3 string)");
      create_table = create_table_temp ;
      createOnlyOnce = false;
  } else if(! createDupTable  ){
      std::string create_table_temp("create or replace table test_small_put(c1 number"
                                    ", c2 number, c3 string)");
      create_table = create_table_temp ;
  }
  if(! create_table.empty()) {
      ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
      assert_int_equal(SF_STATUS_SUCCESS, ret);
  }

  std::string dataDir = TestSetup::getDataDir();
  std::string file = dataDir + fileName;
  replaceStrAll(file, "\\", "\\\\");
  std::string putCommand = "put 'file://" + file + "' @%test_small_put";
  if(createDupTable)
  {
      std::string fileNameStr(fileName);
      replaceStrAll(fileNameStr, "\\", "\\\\");
      putCommand = "put 'file://" + fileNameStr + "' @%test_small_put_dup";
  }
  if (!autoCompress)
  {
    putCommand += " auto_compress=false";
  }

  putCommand += " source_compression=";
  putCommand += sourceCompression;


  if (setCustomThreshold)
  {
    putCommand += " threshold=";
    putCommand += std::to_string(customThreshold);
  }

  std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
    <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

  ITransferResult * results = NULL;
  //catch exception to continue with other test cases if query fails.
  try
  {
    results = agent.execute(&putCommand);
    assert_int_equal(1, results->getResultSize());
  }
  catch (...)
  {
    assert_string_equal("", sfstmt->error.msg);
  }

  while(results->next())
  {
    std::string value;
    results->getColumnAsString(0, value); // source
    assert_string_equal( sf_filename_from_path(fileName), value.c_str());

    std::string expectedTarget = (autoCompress && !strstr(fileName, ".gz")) ?
                                 std::string(fileName) + ".gz" :
                                 std::string(fileName);
    results->getColumnAsString(1, value); // get target
    assert_string_equal(sf_filename_from_path(expectedTarget.c_str()), value.c_str());

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
    if(createDupTable){
        copyCommand = "copy into test_small_put_dup from @%test_small_put_dup";
    }
    ret = snowflake_query(sfstmt, copyCommand.c_str(), copyCommand.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    if(verifyCopyUploadFile) {
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

  }

  if(copyTableToStaging)
  {
      const char* f=NULL;
      std::string copyCommand = "copy into @%test_small_put/bigFile.csv.gz from test_small_put ";
      ret = snowflake_query(sfstmt, copyCommand.c_str(), copyCommand.size());
      assert_int_equal(SF_STATUS_SUCCESS, ret);

      //GS splits the file into multiple pieces.
      std::string listFiles="list @%test_small_put pattern='bigFile.*'";
      ret = snowflake_query(sfstmt, listFiles.c_str(), listFiles.size());
      assert_int_equal(SF_STATUS_SUCCESS, ret);

      int nsplits = snowflake_num_rows(sfstmt);
      //Get the list of those pieces
      for(int i=0; i<nsplits; i++) {
          ret = snowflake_fetch(sfstmt);
          assert_int_equal(SF_STATUS_SUCCESS, ret);
          snowflake_column_as_const_str(sfstmt, 1, &f);
          fileList.emplace_back(f);
      }
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
    
    ITransferResult * results = NULL;
    try
    {
      results = agent.execute(&getcmd);
    }
    catch (...)
    {
      assert_string_equal("", sfstmt->error.msg);
    }

    while(results && results->next())
    {
        results->getColumnAsString(1, get_status);
        //Compressed File sizes vary on Windows/Linux, So not verifying size.
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
  const char *cloud_provider = std::getenv("CLOUD_PROVIDER");
  if(cloud_provider && (strcmp(cloud_provider, "AWS") == 0)) {
      errno = 0;
      return;
  }

  std::string destinationfile="large_file.csv.gz";
  std::string destFile = TestSetup::getDataDir() + destinationfile;
  test_simple_put_core(destinationfile.c_str(), // filename
                       "gzip", //source compression
                       false,   // auto compress
                       true,   // Load data into table
                       false,  // Run select * on loaded table (Not good for large data set)
                       true    // copy data from Table to Staging.
  );
}

void test_large_put_threshold(void **unused)
{
  const char *cloud_provider = std::getenv("CLOUD_PROVIDER");
  if(cloud_provider && (strcmp(cloud_provider, "AWS") == 0)) {
      errno = 0;
      return;
  }
  std::string destinationfile="large_file.csv.gz";
  std::string destFile = TestSetup::getDataDir() + destinationfile;
  test_simple_put_core(destinationfile.c_str(), // filename
                       "gzip", //source compression
                       false,   // auto compress
                       true,   // Load data into table
                       false,  // Run select * on loaded table (Not good for large data set)
                       true,    // copy data from Table to Staging.
                       true,
                       20*1024*1024
  );
}

void test_large_reupload(void **unused)
{
    const char *cloud_provider = std::getenv("CLOUD_PROVIDER");
    if(cloud_provider && (strcmp(cloud_provider, "AWS") == 0)) {
        errno = 0;
        return;
    }
    //Before re-upload delete the already existing staged files.
    SF_STATUS status;
    SF_CONNECT *sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    assert_int_equal(SF_STATUS_SUCCESS, status);

    SF_STMT *sfstmt = NULL;
    SF_STATUS ret;

    /* query */
    sfstmt = snowflake_stmt(sf);

    //This deletes all the files on the stage.
    std::string deleteStagedFiles("REMOVE @%test_small_put/ ;");
    ret = snowflake_query(sfstmt, deleteStagedFiles.c_str(), deleteStagedFiles.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    char tempDir[MAX_PATH] = { 0 };
    char tempFile[MAX_PATH + 256] ={0};
    sf_get_tmp_dir(tempDir);

    for(const std::string s : fileList) {
        tempFile[0] = 0;
        sprintf(tempFile, "%s%c%s", tempDir, PATH_SEP, s.c_str());
        test_simple_put_core(tempFile, // filename
                             "gzip", //source compression
                             false,   // auto compress
                             true,   // Load data into table
                             false,  // Run select * on loaded table (Not good for large data set)
                             false,    // copy data from Table to Staging.
                             true       //Creates a dup table to compare uploaded data.
        );
    }

}

/*
 * Create Table
 * Upload large file to Table Staging Area
 * Copy from staging area into table
 * Copy from Table to Staging area
 * Download files from Staging Area
 * Create Dup Table
 * Upload just downloaded files to Staging Area
 * Copy from Staging area into dup table
 * Make sure TableA - TableB == TableB - TableA == 0
 */
void test_verify_upload(void **unused)
{
    const char *cloud_provider = std::getenv("CLOUD_PROVIDER");
    if(cloud_provider && (strcmp(cloud_provider, "AWS") == 0)) {
        errno = 0;
        return;
    }
    /* init */
    SF_STATUS status;
    SF_CONNECT *sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    assert_int_equal(SF_STATUS_SUCCESS, status);

    SF_STMT *sfstmt = NULL;
    SF_STATUS ret;

    /* query */
    sfstmt = snowflake_stmt(sf);

    //This verifies test_put_get - test_put_get_dup
    std::string VerifyTable("select * from TEST_SMALL_PUT MINUS select * from TEST_SMALL_PUT_DUP;");
    ret = snowflake_query(sfstmt, VerifyTable.c_str(), VerifyTable.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    //As both the tables must have the same data Minus should return 0 rows.
    assert_int_equal(snowflake_num_rows(sfstmt), 0);

    //This verifies test_put_get_dup - test_put_get
    VerifyTable=("select * from TEST_SMALL_PUT_DUP MINUS select * from TEST_SMALL_PUT;");
    ret = snowflake_query(sfstmt, VerifyTable.c_str(), VerifyTable.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    assert_int_equal(snowflake_num_rows(sfstmt), 0);

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

void test_simple_put_gzip_caseInsensitive(void **unused)
{
  //AUTO_COMPRESS=FALSE SOURCE_COMPRESSION=GZIP
  test_simple_put_core("small_file.csv.gz", "GziP", false);
}

void test_simple_put_zero_byte(void **unused)
{
  test_simple_put_core("zero_byte.csv", "auto", true, false);
}

void test_simple_put_one_byte(void **unused)
{
  test_simple_put_core("one_byte.csv", "auto", true, false);
}

void test_simple_put_threshold(void **unused)
{
  test_simple_put_core("small_file.csv.gz", "gzip", false, false, false, false, false, true, 100*1024*1024);
}

void test_simple_get(void **unused)
{
  char tempDir[MAX_PATH] = { 0 };
  char tempPath[MAX_PATH + 256] ="get @%test_small_put/small_file.csv.gz 'file://";
  sf_get_tmp_dir(tempDir);
  std::string tempDirStr = tempDir;
  replaceStrAll(tempDirStr, "\\", "\\\\");
  strcat(tempPath, tempDirStr.c_str());
  strcat(tempPath, "'");
  test_simple_get_data(tempPath, "48");
}

void test_large_get(void **unused)
{
  char tempDir[MAX_PATH] = { 0 };
  char tempPath[MAX_PATH + 256] = "get @%test_small_put/bigFile.csv.gz 'file://";
  const char *cloud_provider = std::getenv("CLOUD_PROVIDER");
  if(cloud_provider && (strcmp(cloud_provider, "AWS") == 0)) {
      errno = 0;
      return;
  }
  sf_get_tmp_dir(tempDir);
  std::string tempDirStr = tempDir;
  replaceStrAll(tempDirStr, "\\", "\\\\");
  strcat(tempPath, tempDirStr.c_str());
  strcat(tempPath, "'");
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
  replaceStrAll(file, "\\", "\\\\");
  std::string putCommand = "put 'file://" + file + "' @%test_small_put";

  std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
    <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

  // load first time should return uploaded
  std::string put_status;

  ITransferResult * results = NULL;
  try
  {
    results = agent.execute(&putCommand);
  }
  catch (...)
  {
    assert_string_equal("", sfstmt->error.msg);
  }

  while(results->next())
  {
    results->getColumnAsString(6, put_status);
    assert_string_equal("UPLOADED", put_status.c_str());
  }

  // Skip is not available on GCP
  bool canSkip = true;
  const char *cloud_provider = std::getenv("CLOUD_PROVIDER");
  if(cloud_provider && ( strcmp(cloud_provider, "GCP") == 0 ) ) {
    canSkip = false;
  }

  // load second time should return skipped
  try
  {
    results = agent.execute(&putCommand);
  }
  catch (...)
  {
    assert_string_equal("", sfstmt->error.msg);
  }

  while(results->next())
  {
    results->getColumnAsString(6, put_status);
    if (canSkip)
    {
      assert_string_equal("SKIPPED", put_status.c_str());
    }
    else
    {
      assert_string_equal("UPLOADED", put_status.c_str());
    }
  }

  snowflake_stmt_term(sfstmt);

  /* close and term */
  snowflake_term(sf); // purge snowflake context

}

void test_simple_put_overwrite(void **unused)
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
    replaceStrAll(file, "\\", "\\\\");
    std::string putCommand = "put 'file://" + file + "' @%test_small_put OVERWRITE=true";

    std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
            <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
    Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

    // load first time should return uploaded
    std::string put_status;
    ITransferResult * results = NULL;
    try
    {
      results = agent.execute(&putCommand);
    }
    catch (...)
    {
      assert_string_equal("", sfstmt->error.msg);
    }
    while(results->next())
    {
        results->getColumnAsString(6, put_status);
        assert_string_equal("UPLOADED", put_status.c_str());
    }

    // load second time should return UPLOADED
    try
    {
      results = agent.execute(&putCommand);
    }
    catch (...)
    {
      assert_string_equal("", sfstmt->error.msg);
    }
    while(results->next())
    {
        results->getColumnAsString(6, put_status);
        assert_string_equal("UPLOADED", put_status.c_str());
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
    cmocka_unit_test_teardown(test_simple_put_gzip_caseInsensitive, teardown),
    cmocka_unit_test_teardown(test_simple_put_zero_byte, teardown),
    cmocka_unit_test_teardown(test_simple_put_one_byte, teardown),
    cmocka_unit_test_teardown(test_simple_put_skip, teardown),
    cmocka_unit_test_teardown(test_simple_put_overwrite, teardown),
    cmocka_unit_test_teardown(test_simple_put_skip, donothing),
    cmocka_unit_test_teardown(test_simple_get, teardown),
    cmocka_unit_test_teardown(test_large_put_auto_compress, donothing),
    cmocka_unit_test_teardown(test_large_get, donothing),
    cmocka_unit_test_teardown(test_large_reupload, donothing),
    cmocka_unit_test_teardown(test_verify_upload, teardown)
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
  return ret;
}
