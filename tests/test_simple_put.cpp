/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <aws/core/Aws.h>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <snowflake/client.h>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "snowflake/IStatementPutGet.hpp"
#include "StatementPutGet.hpp"
#include "FileTransferAgent.hpp"
#include "boost/filesystem.hpp"

#define COLUMN_STATUS "STATUS"
#define COLUMN_SOURCE "SOURCE"
#define COLUMN_TARGET "TARGET"
#define COLUMN_SOURCE_COMPRESSION "SOURCE_COMPRESSION"
#define COLUMN_TARGET_COMPRESSION "TARGET_COMPRESSION"
#define COLUMN_ENCRYPTION "encryption"

#define FILE_NAME_2GB "large_2GBfile.csv"
#define FILE_SIZE_2GB (((size_t)1 << 31) + 1)

#define MAX_BUF_SIZE 4096

using namespace ::Snowflake::Client;
using namespace boost::filesystem;

// use encoding directly instead of actual character to avoid
// build issue with encoding on different platforms
// it's character é which is 0xe9 in Windows-1252 and 0xc3 0xa9 in UTF-8
// On windows the default encoding is Windows-1252 on Linux/Mac it's UTF-8
#ifdef _WIN32
static std::string PLATFORM_STR = "\xe9";
#else
static std::string  PLATFORM_STR = "\xc3\xa9";
#endif
static std::string UTF8_STR = "\xc3\xa9";

bool replaceInPlace( std::string& str, std::string const& replaceThis, std::string const& withThis ) {
    bool replaced = false;
    std::size_t i = str.find( replaceThis );
    while( i != std::string::npos ) {
        replaced = true;
        str = str.substr( 0, i ) + withThis + str.substr( i+replaceThis.size() );
        if( i < str.size()-withThis.size() )
            i = str.find( replaceThis, i+withThis.size() );
        else
            i = std::string::npos;
    }
    return replaced;
}

namespace Snowflake
{
namespace Client
{
class StatementPutGetUnicode : public Snowflake::Client::StatementPutGet
{
public:
  StatementPutGetUnicode(SF_STMT *stmt) : StatementPutGet(stmt) {}
  virtual std::string UTF8ToPlatformString(const std::string& utf8_str)
  {
    std::string result = utf8_str;
    replaceInPlace(result, UTF8_STR, PLATFORM_STR);
    return result;
  }

  virtual std::string platformStringToUTF8(const std::string& platform_str)
  {
    std::string result = platform_str;
    replaceInPlace(result, PLATFORM_STR, UTF8_STR);
    return result;
  }
};

}
}

//File list to be made available to re-upload.
static std::vector<std::string> fileList;
static bool createOnlyOnce = true;

#ifdef _WIN32
void getLongTempPath(char *buffTmpDir)
{
	char longtmpDir[MAX_BUF_SIZE] = { 0 };
	int retL = GetLongPathNameA(buffTmpDir, longtmpDir, MAX_BUF_SIZE);
	if (retL == 0)
	{
		std::cout << "GetLongPathName failed: \n" << GetLastError() << std::endl;
	}
	std::cout << "Short path is : " << buffTmpDir << std::endl;
	std::cout << "Long  path is : " << longtmpDir << std::endl;
	sf_strncpy(buffTmpDir, MAX_BUF_SIZE, longtmpDir, MAX_BUF_SIZE);
}
#endif

void test_large_get(void **);

void test_simple_put_core(const char * fileName,
                          const char * sourceCompression,
                          bool autoCompress,
                          bool copyUploadFile=true,
                          bool verifyCopyUploadFile=true,
                          bool copyTableToStaging=false,
                          bool createDupTable=false,
                          bool setCustomThreshold=false,
                          size_t customThreshold=64*1024*1024,
                          bool useDevUrand=false,
                          bool createSubfolder=false,
                          char * tmpDir = nullptr,
                          bool useS3regionalUrl = false,
                          int compressLevel = -1,
                          bool overwrite = false,
                          SF_CONNECT * connection = nullptr,
                          bool testUnicode = false,
                          bool native = false)
{
  /* init */
  SF_STATUS status;
  SF_CONNECT *sf;

  if (!connection) {
    sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    assert_int_equal(SF_STATUS_SUCCESS, status);
  }
  else {
    // use the connection passed from test case
    sf = connection;
  }

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
  size_t srcSize = 0;
  if (is_regular_file(file))
  {
    srcSize = file_size(file);
  }
  else if (is_regular_file(fileName))
  {
    srcSize = file_size(fileName);
  }
  std::string putCommand = "put file://" + file + " @%test_small_put";
  if (testUnicode)
  {
    replaceInPlace(file, "\\", "\\\\");
    putCommand = "put 'file://" + file + "' @%test_small_put";
  }

  if(createDupTable)
  {
      putCommand = "put file://" + std::string(fileName) + " @%test_small_put_dup";
  }
  else if (createSubfolder)
  {
       putCommand = "put file://" + file + " @%test_small_put/subfolder";
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

  if (overwrite)
  {
      putCommand += " overwrite=true";
  }
  std::unique_ptr<IStatementPutGet> stmtPutGet;
  if (!native)
  {
    if (testUnicode)
    {
      stmtPutGet = std::unique_ptr
        <StatementPutGetUnicode>(new Snowflake::Client::StatementPutGetUnicode(sfstmt));
    }
    else
    {
      stmtPutGet = std::unique_ptr
        <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
    }
  }

  TransferConfig transConfig;
  TransferConfig * transConfigPtr = nullptr;
  if (tmpDir)
  {
    if (native)
    {
      snowflake_set_attribute(sf, SF_CON_PUT_TEMPDIR, tmpDir);
    }
    else
    {
      transConfig.tempDir = tmpDir;
      transConfigPtr = &transConfig;
    }
  }
  if(useS3regionalUrl)
  {
    if (native)
    {
      std::string cmd = "alter session set ENABLE_STAGE_S3_PRIVATELINK_FOR_US_EAST_1=true";
      snowflake_query(sfstmt, cmd.c_str(), cmd.size());
    }
    else
    {
      transConfig.useS3regionalUrl = true;
      transConfigPtr = &transConfig;
    }
  }
  if(compressLevel > 0)
  {
    if (native)
    {
      int8 lv = (int8)compressLevel;
      snowflake_set_attribute(sf, SF_CON_PUT_COMPRESSLV, &lv);
    }
    else
    {
      transConfig.compressLevel = compressLevel;
      transConfigPtr = &transConfig;
    }
  }

  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get(), transConfigPtr);

  if(useDevUrand){
    if (native)
    {
      sf_bool use_urand = SF_BOOLEAN_TRUE;
      snowflake_set_attribute(sf, SF_CON_PUT_USE_URANDOM_DEV, &use_urand);
    }
    else
    {
      agent.setRandomDeviceAsUrand(true);
    }
  }

  std::string expectedSrc = sf_filename_from_path(fileName);
  std::string expectedTarget = (autoCompress && !strstr(expectedSrc.c_str(), ".gz")) ?
                               expectedSrc + ".gz" : expectedSrc;
  std::string expectedSourceCompression = !strstr(fileName, ".gz") ? "none" : "gzip";
  std::string expectedTargetCompression = (!autoCompress && !strstr(fileName, ".gz")) ? "none" : "gzip";

  if (native)
  {
    ret = snowflake_query(sfstmt, putCommand.c_str(), putCommand.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    const char *out;
    int8 out_int8;
    int32 out_int32;
    int64 out_int64;
    uint8 out_uint8;
    uint32 out_uint32;
    uint64 out_uint64;
    float32 out_float32;
    float64 out_float64;
    sf_bool out_bool;
    SF_TIMESTAMP out_timestamp;

    // source
    snowflake_column_as_const_str(sfstmt, 1, &out);
    assert_string_equal(expectedSrc.c_str(), out);
    // special behavior of int8/uint8 get first character of string
    snowflake_column_as_int8(sfstmt, 1, &out_int8);
    assert_int_equal(out_int8, (int8)expectedSrc[0]);
    snowflake_column_as_uint8(sfstmt, 1, &out_uint8);
    assert_int_equal(out_uint8, (uint8)expectedSrc[0]);

    // target
    snowflake_column_as_const_str(sfstmt, 2, &out);
    assert_string_equal(expectedTarget.c_str(), out);

    // source size with all retrieval methods
    // don't varify size with auto compression as the source size
    // would be compressed size.
    if (!autoCompress)
    {
      snowflake_column_as_int32(sfstmt, 3, &out_int32);
      assert_int_equal(out_int32, (int32)srcSize);
      snowflake_column_as_int64(sfstmt, 3, &out_int64);
      assert_int_equal(out_int64, (int64)srcSize);
      snowflake_column_as_uint32(sfstmt, 3, &out_uint32);
      assert_int_equal(out_uint32, (uint32)srcSize);
      snowflake_column_as_uint64(sfstmt, 3, &out_uint64);
      assert_int_equal(out_uint64, (uint64)srcSize);
      snowflake_column_as_float32(sfstmt, 3, &out_float32);
      assert_int_equal(out_float32, (float32)srcSize);
      snowflake_column_as_float64(sfstmt, 3, &out_float64);
      assert_int_equal(out_float64, (float64)srcSize);
    }
    ret = snowflake_column_as_boolean(sfstmt, 3, &out_bool);
    assert_int_equal(ret, SF_STATUS_ERROR_CONVERSION_FAILURE);
    ret = snowflake_column_as_timestamp(sfstmt, 3, &out_timestamp);
    assert_int_equal(ret, SF_STATUS_ERROR_CONVERSION_FAILURE);
    snowflake_column_is_null(sfstmt, 3, &out_bool);
    assert_int_equal(out_bool, SF_BOOLEAN_FALSE);

    // source comparession
    snowflake_column_as_const_str(sfstmt, 5, &out);
    assert_string_equal(expectedSourceCompression.c_str(), out);
    // target compression
    snowflake_column_as_const_str(sfstmt, 6, &out);
    assert_string_equal(expectedTargetCompression.c_str(), out);
    snowflake_column_as_const_str(sfstmt, 7, &out);
    // status
    assert_string_equal("UPLOADED", out);
    // encryption
    snowflake_column_as_const_str(sfstmt, 8, &out);
    assert_string_equal("ENCRYPTED", out);

    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_EOF, ret);
  }
  else
  {
    ITransferResult * results = agent.execute(&putCommand);
    assert_int_equal(1, results->getResultSize());

    while(results->next())
    {
      std::string value;
      results->getColumnAsString(0, value); // source
      assert_string_equal(expectedSrc.c_str(), value.c_str());

      results->getColumnAsString(1, value); // get target
      assert_string_equal(expectedTarget.c_str(), value.c_str());

      results->getColumnAsString(4, value); // get source_compression
      assert_string_equal(expectedSourceCompression.c_str(), value.c_str());

      results->getColumnAsString(5, value); // get target_compression
      assert_string_equal(expectedTargetCompression.c_str(), value.c_str());

      results->getColumnAsString(6, value); // get encryption
      assert_string_equal("UPLOADED", value.c_str());

      results->getColumnAsString(7, value); // get encryption
      assert_string_equal("ENCRYPTED", value.c_str());
    }
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
  if (!connection) {
    snowflake_term(sf); // purge snowflake context
  }
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

  truncate = "drop table if exists test_small_put_dup";
  snowflake_query(sfstmt, truncate.c_str(), truncate.size());
  createOnlyOnce = true;
  fileList.clear();

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
  return 0;
}

void test_simple_get_data(const char *getCommand, const char *size,
                          long getThreshold = 0, bool testUnicode = false,
                          bool native = false)
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

    std::unique_ptr<IStatementPutGet> stmtPutGet;
    if (!native)
    {
      if (testUnicode)
      {
        stmtPutGet = std::unique_ptr
          <StatementPutGetUnicode>(new Snowflake::Client::StatementPutGetUnicode(sfstmt));
      }
      else
      {
        stmtPutGet = std::unique_ptr
          <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
      }
    }

    TransferConfig transConfig;
    TransferConfig * transConfigPtr = nullptr;
    if (getThreshold > 0)
    {
      if (native)
      {
        int64 threshold = getThreshold;
        status = snowflake_set_attribute(sf, SF_CON_GET_THRESHOLD, &threshold);
        assert_int_equal(SF_STATUS_SUCCESS, status);
      }
      else
      {
        transConfig.getSizeThreshold = getThreshold;
        transConfigPtr = &transConfig;
      }
    }

    if (native)
    {
      status = snowflake_query(sfstmt, getCommand, strlen(getCommand));
      assert_int_equal(SF_STATUS_SUCCESS, status);

      while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS)
      {
        const char *out;
        // source
        snowflake_column_as_const_str(sfstmt, 2, &out);
        //Compressed File sizes vary on Windows/Linux, So not verifying size.
        snowflake_column_as_const_str(sfstmt, 3, &out);
        assert_string_equal("DOWNLOADED", out);
        snowflake_column_as_const_str(sfstmt, 4, &out);
        assert_string_equal("DECRYPTED", out);
      }
      assert_int_equal(SF_STATUS_EOF, status);
    }
    else
    {
      Snowflake::Client::FileTransferAgent agent(stmtPutGet.get(), transConfigPtr);

      // load first time should return uploaded
      std::string get_status;
      std::string getcmd(getCommand);
      ITransferResult * results = agent.execute(&getcmd);
      while(results && results->next())
      {
          results->getColumnAsString(1, get_status);
          //Compressed File sizes vary on Windows/Linux, So not verifying size.
          results->getColumnAsString(2, get_status);
          assert_string_equal("DOWNLOADED", get_status.c_str());
          results->getColumnAsString(3, get_status);
          assert_string_equal("DECRYPTED", get_status.c_str());
      }
    }

    snowflake_stmt_term(sfstmt);

    /* close and term */
    snowflake_term(sf); // purge snowflake context

}

void test_large_put_auto_compress_core(bool native)
{
  std::string destinationfile="large_file.csv.gz";
  std::string destFile = TestSetup::getDataDir() + destinationfile;
  test_simple_put_core(destinationfile.c_str(), // filename
                       "gzip", //source compression
                       false,   // auto compress
                       true,   // Load data into table
                       false,  // Run select * on loaded table (Not good for large data set)
                       true,   // copy data from Table to Staging.
                       false, // createDupTable
                       false, // setCustomThreshold
                       64*1024*1024, // customThreshold
                       false, // useDevUrand
                       false, // createSubfolder
                       nullptr, // tmpDir
                       false, // useS3regionalUrl
                       -1, //compressLevel
                       false, // overwrite
                       nullptr, // connection
                       false, // testUnicode
                       native
  );
}

void test_large_put_auto_compress(void **unused)
{
  test_large_put_auto_compress_core(false);
}

void test_large_put_auto_compress_native(void **unused)
{
  test_large_put_auto_compress_core(true);
}

void test_large_put_threshold(void **unused)
{
  std::string destinationfile="large_file.csv.gz";
  std::string destFile = TestSetup::getDataDir() + destinationfile;
  test_simple_put_core(destinationfile.c_str(), // filename
                       "gzip", //source compression
                       false,   // auto compress
                       false,   // Load data into table
                       false,  // Run select * on loaded table (Not good for large data set)
                       false,    // copy data from Table to Staging.
                       false, // createDupTable
                       true, // setCustomThreshold
                       20*1024*1024
  );
}

void test_large_reupload_core(bool native)
{
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

    char tempDir[MAX_BUF_SIZE] = { 0 };
    char tempFile[MAX_BUF_SIZE] ={0};
    sf_get_tmp_dir(tempDir);
#ifdef _WIN32
    getLongTempPath(tempDir);
#endif
    for(const std::string s : fileList) {
        tempFile[0] = 0;
        sprintf(tempFile, "%s%c%s", tempDir, PATH_SEP, s.c_str());
        test_simple_put_core(tempFile, // filename
                             "gzip", //source compression
                             false,   // auto compress
                             true,   // Load data into table
                             false,  // Run select * on loaded table (Not good for large data set)
                             false,    // copy data from Table to Staging.
                             true,       //Creates a dup table to compare uploaded data.
                             false, // setCustomThreshold
                             64*1024*1024, // customThreshold
                             false, // useDevUrand
                             false, // createSubfolder
                             nullptr, // tmpDir
                             false, // useS3regionalUrl
                             -1, //compressLevel
                             false, // overwrite
                             nullptr, // connection
                             false, // testUnicode
                             native
        );
    }
}

void test_large_reupload(void** unused)
{
  test_large_reupload_core(false);
}

void test_large_reupload_native(void** unused)
{
  test_large_reupload_core(true);
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

void test_simple_put_use_dev_urandom_core(bool native)
{
  std::string dataDir = TestSetup::getDataDir();
  std::string file = dataDir + "medium_file.csv";

  FILE *fp = fopen(file.c_str(), "w") ;
  for(int i = 0; i < 200000 ; ++i)
  {
    fprintf(fp, "%d,%d,ABCDEFGHIJKLMNOPQRSTUVWXYZ\n",i,i+1);
  }
  fclose(fp);

  /* init */
  SF_STATUS status;
  SF_CONNECT *sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  assert_int_equal(SF_STATUS_SUCCESS, status);

  SF_STMT *sfstmt = NULL;
  SF_STATUS ret;

  /* query */
  sfstmt = snowflake_stmt(sf);
  std::string create_table = "create or replace table test_small_put(c1 number, c2 number, c3 string)" ;
  ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  std::string putCommand = "put file://" + file + " @%test_small_put auto_compress=true overwrite=true";

  if (native)
  {
    sf_bool useUrand = SF_BOOLEAN_TRUE;
    ret = snowflake_set_attribute(sf, SF_CON_PUT_USE_URANDOM_DEV, &useUrand);
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    ret = snowflake_query(sfstmt, putCommand.c_str(), putCommand.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    const char* out = NULL;
    // source
    snowflake_column_as_const_str(sfstmt, 1, &out);
    assert_string_equal(sf_filename_from_path(file.c_str()), out);
    // target
    snowflake_column_as_const_str(sfstmt, 2, &out);
    assert_string_equal("medium_file.csv.gz", out);
    // source compression
    snowflake_column_as_const_str(sfstmt, 5, &out);
    assert_string_equal("none", out);
    // status
    snowflake_column_as_const_str(sfstmt, 7, &out);
    assert_string_equal("UPLOADED", out);
    // encryption
    snowflake_column_as_const_str(sfstmt, 8, &out);
    assert_string_equal("ENCRYPTED", out);

    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_EOF, ret);
  }
  else
  {
    std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
      <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));

    Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());
    agent.setRandomDeviceAsUrand(true);

    ITransferResult * results = agent.execute(&putCommand);
    assert_int_equal(1, results->getResultSize());

    while (results->next())
    {
      std::string value;
      results->getColumnAsString(0, value); // source
      assert_string_equal(sf_filename_from_path(file.c_str()), value.c_str());

      results->getColumnAsString(1, value); // get target
      assert_string_equal("medium_file.csv.gz", value.c_str());

      results->getColumnAsString(4, value); // get source_compression
      assert_string_equal("none", value.c_str());

      results->getColumnAsString(6, value); // get encryption
      assert_string_equal("UPLOADED", value.c_str());

      results->getColumnAsString(7, value); // get encryption
      assert_string_equal("ENCRYPTED", value.c_str());
    }
  }

  std::string copyCommand = "copy into test_small_put from @%test_small_put";
  ret = snowflake_query(sfstmt, copyCommand.c_str(), copyCommand.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  std::string selectCommand = "select * from test_small_put";
  ret = snowflake_query(sfstmt, selectCommand.c_str(), selectCommand.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  const char *out_c1;
  const char *out_c2;
  const char *out_c3;
  assert_int_equal(snowflake_num_rows(sfstmt), 200000);

  for(int i = 0; i < 200000; ++i)
  {
    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    snowflake_column_as_const_str(sfstmt, 1, &out_c1);
    snowflake_column_as_const_str(sfstmt, 2, &out_c2);
    snowflake_column_as_const_str(sfstmt, 3, &out_c3);
    std::string c1 = std::to_string(i);
    std::string c2 = std::to_string(i + 1);
    assert_string_equal(out_c1, c1.c_str());
    assert_string_equal(out_c2, c2.c_str());
    assert_string_equal(out_c3, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    out_c1 = NULL;
    out_c2 = NULL;
    out_c3 = NULL;
  }
  ret = snowflake_fetch(sfstmt);
  assert_int_equal(SF_STATUS_EOF, ret);

}

void test_simple_put_use_dev_urandom(void **unused)
{
  test_simple_put_use_dev_urandom_core(false);
}

void test_simple_put_use_dev_urandom_native(void **unused)
{
  test_simple_put_use_dev_urandom_core(true);
}


void test_simple_put_auto_compress_core(bool native)
{
  test_simple_put_core("small_file.csv", // filename
                       "auto", //source compression
                       true, // auto compress
                       true, // copyUploadFile
                       true, // verifyCopyUploadFile
                       false, // copyTableToStaging
                       false, // createDupTable
                       false, // setCustomThreshold
                       64*1024*1024, // customThreshold
                       false, // useDevUrand
                       false, // createSubfolder
                       nullptr, // tmpDir
                       false, // useS3regionalUrl
                       -1, //compressLevel
                       false, // overwrite
                       nullptr, // connection
                       false, // testUnicode
                       native
  );

  test_simple_put_core("small_file.csv", // filename
                       "auto", //source compression
                       true, // auto compress
                       false,
                       false,
                       false,
                       false,
                       false,
                       100*1024*1024,
                       false,
                       false,
                       nullptr,
                       false,
                       1,
                       true, // overwrite
                       nullptr, // connection
                       false, // testUnicode
                       native
  );

  test_simple_put_core("small_file.csv", // filename
                       "auto", //source compression
                       true, // auto compress
                       false,
                       false,
                       false,
                       false,
                       false,
                       100*1024*1024,
                       false,
                       false,
                       nullptr,
                       false,
                       9,
                       true, // overwrite
                       nullptr, // connection
                       false, // testUnicode
                       native
  );
}

void test_simple_put_auto_compress(void **unused)
{
  test_simple_put_auto_compress_core(false);
}

void test_simple_put_auto_compress_native(void **unused)
{
  test_simple_put_auto_compress_core(true);
}

void test_simple_put_config_temp_dir_core(bool native)
{
  char tmpDir[MAX_PATH] = {0};
  char tmpDirInjection[MAX_PATH] = {0};
  char pathSepStr[2] = {PATH_SEP, '\0'};
  sf_get_tmp_dir(tmpDir);
  strcat(tmpDir, "test_config_temp_dir");
  strcat(tmpDir, pathSepStr);
  strcat(tmpDir, "test_subfolder");

  if (sf_is_directory_exist(tmpDir))
  {
    sf_delete_directory_if_exists(tmpDir);
  }

  // run with configured temp folder
  test_simple_put_core("small_file.csv", // filename
                       "auto", //source compression
                       true, // auto compress
                       true, // copyUploadFile
                       true, // verifyCopyUploadFile
                       false, // copyTableToStaging
                       false, // createDupTable
                       false, // setCustomThreshold
                       64*1024*1024, // customThreshold
                       false, // useDevUrand
                       false, // createSubfolder
                       tmpDir,
                       false, // useS3regionalUrl
                       -1, //compressLevel
                       false, // overwrite
                       nullptr, // connection
                       false, // testUnicode
                       native
  );

  assert_true(sf_is_directory_exist(tmpDir));

  sf_delete_directory_if_exists(tmpDir);

#ifdef _WIN32
  #define CMD_SEPARATOR "&"
#else
  #define CMD_SEPARATOR ";"
#endif
  // make sure the parent folder exists and the folder
  // for injection test doesn't.
  sf_get_tmp_dir(tmpDir);
  if (!sf_is_directory_exist(tmpDir))
  {
    sf_create_directory_if_not_exists_recursive(tmpDir);
  }
  strcat(tmpDir, "injection");
  if (sf_is_directory_exist(tmpDir))
  {
    sf_delete_directory_if_exists(tmpDir);
  }

  // native execution doesn't throw exception
  if (!native)
  {
    // try injection the command for folder deletion like
    // rm -rf xxx ; mkdir <tmpDir>/injection ; xxx
    sprintf(tmpDirInjection, "xxx %s mkdir %s %s xxx",
            CMD_SEPARATOR, tmpDir, CMD_SEPARATOR);
    try
    {
      test_simple_put_core("small_file.csv", // filename
                           "auto", //source compression
                           true, // auto compress
                           true, // copyUploadFile
                           true, // verifyCopyUploadFile
                           false, // copyTableToStaging
                           false, // createDupTable
                           false, // setCustomThreshold
                           64*1024*1024, // customThreshold
                           false, // useDevUrand
                           false, // createSubfolder
                           tmpDirInjection,
                           false, // useS3regionalUrl
                           -1, //compressLevel
                           false, // overwrite
                           nullptr, // connection
                           false, // testUnicode
                           native
      );
    }
    catch (...)
    {
      //ignore exception as the failure is expected.
    }
    assert_false(sf_is_directory_exist(tmpDir));
    }
}

void test_simple_put_config_temp_dir(void **unused)
{
  test_simple_put_config_temp_dir_core(false);
}

void test_simple_put_config_temp_dir_native(void **unused)
{
  test_simple_put_config_temp_dir_core(true);
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
  test_simple_put_core("small_file.csv.gz", "gzip", false, false, false, false, false, false, 100*1024*1024);
}

void test_simple_put_create_subfolder(void **unused)
{
  test_simple_put_core("small_file.csv.gz", "gzip", false, false, false, false, false, false, 100*1024*1024, false, true);
}

void test_simple_put_use_s3_regionalURL_core(bool native)
{
  test_simple_put_core("small_file.csv.gz", "gzip", false,false,
                       false,
                       false,
                       false,
                       false,
                       100*1024*1024,
                       false,
                       false,
                       nullptr,
                       true, // useS3regionalUrl
                       -1, //compressLevel
                       false, // overwrite
                       nullptr, // connection
                       false, // testUnicode
                       native
  );
}

void test_simple_put_use_s3_regionalURL(void **unused)
{
  test_simple_put_use_s3_regionalURL_core(false);
}

void test_simple_put_use_s3_regionalURL_native(void **unused)
{
  test_simple_put_use_s3_regionalURL_core(true);
}

void test_simple_get_core(bool native)
{
  test_simple_put_core("small_file.csv", // filename
                       "auto", //source compression
                       true, // auto compress
                       true, // copyUploadFile
                       true, // verifyCopyUploadFile
                       false, // copyTableToStaging
                       false, // createDupTable
                       false, // setCustomThreshold
                       64*1024*1024, // customThreshold
                       false, // useDevUrand
                       false, // createSubfolder
                       nullptr, // tmpDir
                       false, // useS3regionalUrl
                       -1, //compressLevel
                       false, // overwrite
                       nullptr, // connection
                       false, // testUnicode
                       native
  );

  char tempDir[MAX_BUF_SIZE] = { 0 };
  char command[MAX_BUF_SIZE] = "get @%test_small_put/small_file.csv.gz file://";
  sf_get_tmp_dir(tempDir);
#ifdef _WIN32
  getLongTempPath(tempDir);
#endif
  strcat(command, tempDir);
  test_simple_get_data(command, // getCommand
                       "48", // size
                       0, // getThreshold
                       false, // testUnicode
                       native
  );
}

void test_simple_get(void **unused)
{
  test_simple_get_core(false);
}

void test_simple_get_native(void **unused)
{
  test_simple_get_core(true);
}

void test_large_get_core(bool native)
{
  char tempDir[MAX_BUF_SIZE] = { 0 };
  char command[MAX_BUF_SIZE] = "get @%test_small_put/bigFile.csv.gz file://";
  sf_get_tmp_dir(tempDir);
#ifdef _WIN32
  getLongTempPath(tempDir);
#endif
  strcat(command, tempDir);
  test_simple_get_data(command, // getCommand
                       "5166848", // size
                       0, // getThreshold
                       false, // testUnicode
                       native
  );
}

void test_large_get(void **unused)
{
  test_large_get_core(false);
}

void test_large_get_native(void **unused)
{
  test_large_get_core(true);
}

void test_large_get_threshold_core(bool native)
{
  char tempDir[MAX_BUF_SIZE] = { 0 };
  char command[MAX_BUF_SIZE] = "get @%test_small_put/bigFile.csv.gz file://";
  sf_get_tmp_dir(tempDir);
#ifdef _WIN32
  getLongTempPath(tempDir);
#endif
  strcat(command, tempDir);
  test_simple_get_data(command, // getCommand
                       "5166848", // size
                       1000000, // getThreshold
                       false, // testUnicode
                       native
  );
}

void test_large_get_threshold(void **unused)
{
  test_large_get_threshold_core(false);
}

void test_large_get_threshold_native(void **unused)
{
  test_large_get_threshold_core(true);
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);

  // TODO SNOW-1526335
  // Sometime we can't get OCSP response from cache server or responder
  // Usually happen on GCP and should be ignored by FAIL_OPEN
  // Unfortunately libsnowflakeclient doesn't support FAIL_OPEN for now
  // so we have to disable OCSP validation to around it.
  // Will remove this code when adding support for FAIL_OPEN (which is
  // the default behavior for all other drivers)
  char *cenv = getenv("CLOUD_PROVIDER");
  if (cenv && !strncmp(cenv, "GCP", 4)) {
    sf_bool value = SF_BOOLEAN_FALSE;
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);
  }

  if(!setup_random_database()) {
    std::cout << "Failed to setup random database, fallback to use regular one." << std::endl;
  }

  // create large 2GB file
  char *githubenv = getenv("GITHUB_ACTIONS");
  if (githubenv && strlen(githubenv) > 0)
  {
    char *cenv = getenv("CLOUD_PROVIDER");
    if (cenv && !strncmp(cenv, "AWS", 4)) {
      return 0;
    }
  }
// Jenkins node on Mac has issue with large file.
#ifdef __APPLE__
  char* jobname = getenv("JOB_NAME");
  if (jobname && (strlen(jobname) > 0))
  {
    return 0;
  }
#endif

  std::string file2GB = TestSetup::getDataDir() + FILE_NAME_2GB;
  std::ofstream ofs(file2GB, std::ios::binary | std::ios::out);
  ofs.seekp(FILE_SIZE_2GB - 1);
  ofs.write("", 1);
  ofs.close();

  return 0;
}

static int gr_teardown(void **unused)
{
  drop_random_database();
  snowflake_global_term();
  std::string file2GB = TestSetup::getDataDir() + FILE_NAME_2GB;
  remove(file2GB.c_str());
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
    std::string putCommand = "put file://" + file + " @%test_small_put OVERWRITE=true";

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

    // load second time should return UPLOADED
    results = agent.execute(&putCommand);
    while(results->next())
    {
        results->getColumnAsString(6, put_status);
        assert_string_equal("UPLOADED", put_status.c_str());
    }

    snowflake_stmt_term(sfstmt);

    /* close and term */
    snowflake_term(sf); // purge snowflake context

}

void test_server_side_encryption(void **unused)
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

    std::string create_stage("create or replace stage test_sse "
                             "encryption = (type = 'SNOWFLAKE_SSE');");
    ret = snowflake_query(sfstmt, create_stage.c_str(), create_stage.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    std::string dataDir = TestSetup::getDataDir();
    std::string file = dataDir + "small_file.csv";
    std::string putCommand = "put file://" + file + " @test_sse";

    std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
            <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
    Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

    // load
    std::string put_status;
    ITransferResult * results = agent.execute(&putCommand);
    while(results->next())
    {
        results->getColumnAsString(6, put_status);
        assert_string_equal("UPLOADED", put_status.c_str());
    }

    // download
    char tempDir[MAX_BUF_SIZE] = { 0 };
    char tempPath[MAX_BUF_SIZE] = "get @test_sse/small_file.csv.gz file://";
    sf_get_tmp_dir(tempDir);
#ifdef _WIN32
    getLongTempPath(tempDir);
#endif
    strcat(tempPath, tempDir);
    std::string get_status;
    std::string getcmd(tempPath);
    results = agent.execute(&getcmd);
    while(results && results->next())
    {
        results->getColumnAsString(1, get_status);
        //Compressed File sizes vary on Windows/Linux, So not verifying size.
        results->getColumnAsString(2, get_status);
        assert_string_equal("DOWNLOADED", get_status.c_str());
        results->getColumnAsString(3, get_status);
        assert_string_equal("DECRYPTED", get_status.c_str());
    }

    std::string drop_stage("drop stage test_sse");
    ret = snowflake_query(sfstmt, drop_stage.c_str(), drop_stage.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);

    snowflake_stmt_term(sfstmt);

    /* close and term */
    snowflake_term(sf); // purge snowflake context
}

void replaceStrAll(std::string &stringToReplace,
                   std::string const &oldValue,
                   std::string const &newValue) {
  size_t oldValueLen = oldValue.length();
  size_t newValueLen = newValue.length();
  if (0 == oldValueLen) {
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

void createFile(const char *file) {
  FILE *fp = fopen(file, "w");
  if (fp != NULL) {
    fprintf(fp, "1,2\n3,4\n5,6");
    fclose(fp);
  } else {
    std::cerr << "Could not open file " << file << " to write" << std::endl;
    return;
  }
  return;
}

void test_simple_put_uploadfail(void **unused) {
  /* init */
  SF_STATUS status;
  SF_CONNECT *sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  assert_int_equal(SF_STATUS_SUCCESS, status);

  SF_STMT *sfstmt = NULL;
  SF_STATUS ret;

  /* query */
  sfstmt = snowflake_stmt(sf);

  std::string create_table("CREATE OR REPLACE STAGE TEST_ODBC_FILE_URIS");
  ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  char tmpDir[MAX_BUF_SIZE] = {0};
  sf_get_tmp_dir(tmpDir);
  sf_get_uniq_tmp_dir(tmpDir);
#ifdef _WIN32
  getLongTempPath(tmpDir);
#endif

  std::vector<std::string> putFilePath = {
    std::string(tmpDir) + PATH_SEP + "fileExist.csv",
    std::string(tmpDir) + PATH_SEP + "fileExist",
    std::string(tmpDir) + PATH_SEP + "file with space.csv",
    std::string(tmpDir) + PATH_SEP + "file%20with%20percent20.csv",
    std::string(tmpDir) + PATH_SEP + "fileExistDat.dat",
  };
#ifdef _WIN32
  std::string str = putFilePath[0];
  replaceStrAll(str,"\\","/");
  replaceStrAll(str, "//", "/");
  putFilePath.push_back(str);

  str = putFilePath[3];
  replaceStrAll(str, "\\", "/");
  replaceStrAll(str, "//", "/");
  putFilePath.push_back(str);

  str = putFilePath[4];
  replaceStrAll(str, "\\", "/");
  replaceStrAll(str, "//", "/");
  putFilePath.push_back(str);

  str = putFilePath[2];
  replaceStrAll(str, "\\", "/");
  replaceStrAll(str, "//", "/");
  putFilePath.push_back(str);
#endif

  for (auto f : putFilePath) {
    createFile(f.c_str());
  }
  typedef struct tcases {
    std::string putcmd;
    const char *result;
  } tcases;

  std::vector<tcases> testCases = {
    { std::string("put file://") + std::string(tmpDir) + std::string("filedoesnotexist.csv @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=FALSE PARALLEL=8 OVERWRITE=true"), "ERROR"},
    { std::string("put file://") + std::string(tmpDir) + std::string("filedoesnotexist.csv @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "ERROR"},
    { std::string("put file://") + std::string(tmpDir) + std::string("filedoesnotexist.csv.gz @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=FALSE PARALLEL=8 OVERWRITE=true"), "ERROR"},
    { std::string("put file://") + putFilePath[0] + std::string(" @STAGE_NOT_EXIST AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "ERROR"},
    { std::string("put file://") + putFilePath[0] + std::string(" @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED"},
  //{ std::string("put file://") + putFilePath[0] + std::string(" @TEST_ODBC_FILE_URIS/bucket AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "ERROR"},
    { std::string("put file://") + putFilePath[1] + std::string(" @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED"},
    { std::string("put file://") + putFilePath[1] + std::string(" @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=FALSE PARALLEL=8 OVERWRITE=true"), "UPLOADED"},
    { std::string("put file://") + putFilePath[1] + std::string(" @~/temp/ AUTO_COMPRESS=FALSE OVERWRITE=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED"},
  //{ std::string("put file://") + putFilePath[2] + std::string(" @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED"},
#ifndef _WIN32
    { std::string("put 'file://") + putFilePath[2] + std::string("' @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED"},
#endif
    { std::string("put file://") + putFilePath[3] + std::string(" @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED"},
    { std::string("put file://") + putFilePath[4] + std::string(" @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED"},
#ifdef _WIN32
    { std::string("put file://") + putFilePath[5] + std::string(" @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED" },
    { std::string("put file://") + putFilePath[6] + std::string(" @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED" },
    { std::string("put file://") + putFilePath[7] + std::string(" @~/temp/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED" },
    { std::string("put 'file://") + putFilePath[8] + std::string("' @TEST_ODBC_FILE_URIS/bucket/ AUTO_COMPRESS=TRUE PARALLEL=8 OVERWRITE=true"), "UPLOADED" },
#endif
  };

  std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
    <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

  for(auto putCommand : testCases)
  {
    std::cout << "TesteCase: " << putCommand.putcmd << std::endl;
    std::string put_status = "ERROR";
    try
    {
      ITransferResult *results = agent.execute(&putCommand.putcmd);
      while (results->next())
      {
        results->getColumnAsString(6, put_status);
        assert_string_equal(putCommand.result, put_status.c_str());
      }
    }
    catch (...)
    {
      assert_string_equal(putCommand.result, put_status.c_str());
    }
  }
  create_table = "DROP STAGE IF EXISTS TEST_ODBC_FILE_URIS";
  ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
  snowflake_stmt_term(sfstmt);

  /* close and term */
  snowflake_term(sf); // purge snowflake context
  sf_delete_directory_if_exists(tmpDir);

}

void test_2GBlarge_put(void **unused)
{
  // on github the existing test case for large file put/get is skipped for
  // aws so do the same. Make it available for manual test though
  // since we do have code changes for all platforms.
  char *githubenv = getenv("GITHUB_ACTIONS");
  char *cenv = getenv("CLOUD_PROVIDER");
  if (githubenv && strlen(githubenv) > 0)
  {
    if (cenv && !strncmp(cenv, "AWS", 4)) {
      errno = 0;
      return;
    }
  }

// Jenkins node on Mac has issue with large file.
#ifdef __APPLE__
  char* jobname = getenv("JOB_NAME");
  if (jobname && (strlen(jobname) > 0))
  {
    return;
  }
#endif

  test_simple_put_core(FILE_NAME_2GB, // filename
                       "none", //source compression
                       false,   // auto compress
                       false,   // Load data into table
                       false,  // Run select * on loaded table
                       false    // copy data from Table to Staging.
  );
}

void test_2GBlarge_get(void **unused)
{
  // on github the existing test case for large file put/get is skipped for
  // aws so do the same. Make it available for manual test though
  // since we do have code changes for all platforms.
  char *githubenv = getenv("GITHUB_ACTIONS");
  char *cenv = getenv("CLOUD_PROVIDER");
  if (githubenv && strlen(githubenv) > 0)
  {
    if (cenv && !strncmp(cenv, "AWS", 4)) {
      errno = 0;
      return;
    }
  }

// Jenkins node on Mac has issue with large file.
#ifdef __APPLE__
  char* jobname = getenv("JOB_NAME");
  if (jobname && (strlen(jobname) > 0))
  {
    return;
  }
#endif

  std::string getcmd = std::string("get @%test_small_put/") + FILE_NAME_2GB +
                       " file://" + TestSetup::getDataDir();
  test_simple_get_data(getcmd.c_str(), std::to_string(FILE_SIZE_2GB).c_str());
}

void test_simple_put_with_proxy(void **unused)
{
  SKIP_IF_PROXY_ENV_IS_SET;

  // set invalid proxy in environment variables
  sf_setenv("https_proxy", "a.b.c");
  sf_setenv("http_proxy", "a.b.c");
  sf_unsetenv("no_proxy");

  SF_CONNECT *sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_PROXY, "");
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  test_simple_put_core("small_file.csv", // filename
                       "auto", //source compression
                       true, // auto compress
                       true, // copyUploadFile
                       true, // verifyCopyUploadFile
                       false, // copyTableToStaging
                       false, // createDupTable
                       false, // setCustomThreshold
                       64 * 1024 * 1024, // customThreshold
                       false, // useDevUrand
                       false, // createSubfolder
                       nullptr, // tmpDir
                       false, // useS3regionalUrl
                       -1, // compressLevel
                       false, // overwrite
                       sf // connection
  );

  snowflake_term(sf);
  sf_unsetenv("https_proxy");
  sf_unsetenv("http_proxy");
}

void test_simple_put_with_noproxy(void **unused)
{
  SKIP_IF_PROXY_ENV_IS_SET;

  // set invalid proxy in environment variables
  sf_setenv("https_proxy", "a.b.c");
  sf_setenv("http_proxy", "a.b.c");
  sf_unsetenv("no_proxy");

  SF_CONNECT *sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_PROXY, "a.b.c");
  snowflake_set_attribute(sf, SF_CON_NO_PROXY, "*");
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  test_simple_put_core("small_file.csv", // filename
    "auto", //source compression
    true, // auto compress
    true, // copyUploadFile
    true, // verifyCopyUploadFile
    false, // copyTableToStaging
    false, // createDupTable
    false, // setCustomThreshold
    64 * 1024 * 1024, // customThreshold
    false, // useDevUrand
    false, // createSubfolder
    nullptr, // tmpDir
    false, // useS3regionalUrl
    -1, // compressLevel
    false, // overwrite
    sf // connection
  );

  snowflake_term(sf);
  sf_unsetenv("https_proxy");
  sf_unsetenv("http_proxy");
}

void test_simple_put_with_proxy_fromenv(void **unused)
{
    SKIP_IF_PROXY_ENV_IS_SET;

    // set invalid proxy settings
    sf_setenv("https_proxy", "a.b.c");
    sf_unsetenv("http_proxy");
    sf_setenv("no_proxy", "a.b.c");
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS); // must fail
    snowflake_term(sf);

    // test PUT command with valid https_proxy setting
    sf_setenv("https_proxy", "");

    sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    test_simple_put_core("small_file.csv", // filename
        "auto", //source compression
        true, // auto compress
        true, // copyUploadFile
        true, // verifyCopyUploadFile
        false, // copyTableToStaging
        false, // createDupTable
        false, // setCustomThreshold
        64 * 1024 * 1024, // customThreshold
        false, // useDevUrand
        false, // createSubfolder
        nullptr, // tmpDir
        false, // useS3regionalUrl
        -1, // compressLevel
        false, // overwrite
        sf // connection
    );

    snowflake_term(sf);
    sf_unsetenv("https_proxy");
    sf_unsetenv("http_proxy");
    sf_unsetenv("no_proxy");
}

void test_simple_put_with_noproxy_fromenv(void **unused)
{
    SKIP_IF_PROXY_ENV_IS_SET;

    // set invalid proxy settings
    sf_setenv("https_proxy", "a.b.c");
    sf_setenv("http_proxy", "a.b.c");
    sf_unsetenv("no_proxy");
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS); // must fail
    snowflake_term(sf);

    // test PUT command with valid no_proxy setting
    sf_setenv("no_proxy", "*");

    sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    test_simple_put_core("small_file.csv", // filename
        "auto", //source compression
        true, // auto compress
        true, // copyUploadFile
        true, // verifyCopyUploadFile
        false, // copyTableToStaging
        false, // createDupTable
        false, // setCustomThreshold
        64 * 1024 * 1024, // customThreshold
        false, // useDevUrand
        false, // createSubfolder
        nullptr, // tmpDir
        false, // useS3regionalUrl
        -1, // compressLevel
        false, // overwrite
        sf // connection
    );

    snowflake_term(sf);
    sf_unsetenv("https_proxy");
    sf_unsetenv("http_proxy");
    sf_unsetenv("no_proxy");
}

std::string getLastModifiedFromStage(SF_STMT * sfstmt)
{
    const char* expectedValue = NULL;
    std::string list_stage = "LIST @testStage";

    SF_STATUS status = snowflake_query(sfstmt, list_stage.c_str(), list_stage.size());
    assert_int_equal(SF_STATUS_SUCCESS, status);

    int numRows = snowflake_num_rows(sfstmt);
    if (numRows > 0)
    {
        status = snowflake_fetch(sfstmt);
        assert_int_equal(SF_STATUS_SUCCESS, status);

        int64 num_fields = snowflake_num_fields(sfstmt);
        assert_int_equal(num_fields, 4);

        SF_COLUMN_DESC *descs = snowflake_desc(sfstmt);
        char* columnName = descs[3].name;
        assert_string_equal(columnName, "last_modified");

        snowflake_column_as_const_str(sfstmt, 4, &expectedValue);

        status = snowflake_fetch(sfstmt);
        assert_int_equal(SF_STATUS_EOF, status);
    }

    if (expectedValue)
    {
        return std::string(expectedValue);
    }

    return std::string();
}

void test_upload_file_to_stage_using_stream(void **unused)
{
    SF_CONNECT* sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    assert_int_equal(SF_STATUS_SUCCESS, status);

    SF_STMT* sfstmt = snowflake_stmt(sf);

    // create a stage for file uploading
    std::string create_stage = "CREATE OR REPLACE STAGE testStage";
    status = snowflake_query(sfstmt, create_stage.c_str(), create_stage.size());
    assert_int_equal(SF_STATUS_SUCCESS, status);

    // note down the last_modified time
    std::string expectedValue = getLastModifiedFromStage(sfstmt);

    // add 1 sec delay between uploads
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::string dataDir = TestSetup::getDataDir();
    std::string fileName = "small_file.csv";
    std::string file = dataDir + fileName;
    std::string putCommand = "put file://" + file + " @testStage auto_compress=false source_compression=gzip overwrite=true";

    std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
        <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));

    TransferConfig transConfig;
    TransferConfig* transConfigPtr = nullptr;

    Snowflake::Client::FileTransferAgent agent(stmtPutGet.get(), transConfigPtr);

    // create stream from the csv file
    std::string line;
    std::ifstream fileStream(file);
    if (fileStream.is_open())
    {
        while (fileStream.good())
        {
            getline(fileStream, line);
        }
        fileStream.close();
    }
    else
    {
        assert_true(!line.empty());
    }
    std::stringstream stringStream = std::stringstream(line);
    stringStream.seekg(0, std::ios::end);
    std::stringstream::pos_type offset = stringStream.tellg();
    agent.setUploadStream(&stringStream, offset);

    ITransferResult * results = agent.execute(&putCommand);
    assert_int_equal(1, results->getResultSize());

    // verify the upload results
    while (results->next())
    {
        std::string value;
        results->getColumnAsString(0, value); // source
        assert_string_equal(sf_filename_from_path(fileName.c_str()), value.c_str());

        results->getColumnAsString(1, value); // get target
        assert_string_equal(file.c_str(), value.c_str());

        results->getColumnAsString(4, value); // get source_compression
        assert_string_equal("gzip", value.c_str());

        results->getColumnAsString(5, value); // get target_compression
        assert_string_equal("gzip", value.c_str());

        results->getColumnAsString(6, value); // get status
        assert_string_equal("UPLOADED", value.c_str());

        results->getColumnAsString(7, value); // get encryption
        assert_string_equal("ENCRYPTED", value.c_str());
    }

    // check the last_modified time again and make sure it has been updated
    std::string actualValue = getLastModifiedFromStage(sfstmt);
    assert_true(expectedValue != actualValue);

    std::string drop_stage = "DROP STAGE if exists testStage";
    status = snowflake_query(sfstmt, drop_stage.c_str(), drop_stage.size());
    assert_int_equal(SF_STATUS_SUCCESS, status);

    snowflake_term(sf);
}

void test_put_get_with_unicode(void **unused)
{
  std::string dataDir = TestSetup::getDataDir();
  std::string filename=PLATFORM_STR + ".csv";
  copy_file(dataDir + "small_file.csv", dataDir + filename, copy_options::overwrite_existing);
  filename = UTF8_STR + ".csv";
  test_simple_put_core(
      filename.c_str(), // filename
      "auto", //source compression
      true, // auto compress
      true, // copyUploadFile
      true, // verifyCopyUploadFile
      false, // copyTableToStaging
      false, // createDupTable
      false, // setCustomThreshold
      64 * 1024 * 1024, // customThreshold
      false, // useDevUrand
      false, // createSubfolder
      nullptr, // tmpDir
      false, // useS3regionalUrl
      -1, // compressLevel
      false, // overwrite
      nullptr, // connection
      true // testUnicode
  );

  std::string getcmd = std::string("get '@%test_small_put/") + UTF8_STR +".csv.gz'"
                       " file://" + TestSetup::getDataDir();
  test_simple_get_data(getcmd.c_str(), "48", 0, true);
}

int main(void) {

#ifdef __APPLE__
  std::string testAccount =  getenv("SNOWFLAKE_TEST_ACCOUNT");

  std::for_each(testAccount.begin(), testAccount.end(), [](char & c) {
      c = ::toupper(c);
      }); 
  if(testAccount.find("GCP") != std::string::npos)
  {
    setenv("CLOUD_PROVIDER", "GCP", 1);
  }
  else if(testAccount.find("AZURE") != std::string::npos)
  {
    setenv("CLOUD_PROVIDER", "AZURE", 1); 
  }
  else
  {
    setenv("CLOUD_PROVIDER", "AWS", 1); 
  }

  char *cp = getenv("CLOUD_PROVIDER");
  std::cout << "Cloud provider is " << cp << std::endl; 
#endif

  const struct CMUnitTest tests[] = {
    cmocka_unit_test_teardown(test_simple_put_auto_compress, teardown),
    cmocka_unit_test_teardown(test_simple_put_config_temp_dir, teardown),
    cmocka_unit_test_teardown(test_simple_put_auto_detect_gzip, teardown),
    cmocka_unit_test_teardown(test_simple_put_no_compress, teardown),
    cmocka_unit_test_teardown(test_simple_put_gzip, teardown),
    cmocka_unit_test_teardown(test_simple_put_gzip_caseInsensitive, teardown),
    cmocka_unit_test_teardown(test_simple_put_threshold, teardown),
    cmocka_unit_test_teardown(test_simple_put_zero_byte, teardown),
    cmocka_unit_test_teardown(test_simple_put_one_byte, teardown),
    cmocka_unit_test_teardown(test_simple_put_skip, teardown),
    cmocka_unit_test_teardown(test_simple_put_overwrite, teardown),
    cmocka_unit_test_teardown(test_simple_get, teardown),
    cmocka_unit_test_teardown(test_large_put_auto_compress, donothing),
    cmocka_unit_test_teardown(test_large_get, donothing),
    cmocka_unit_test_teardown(test_large_get_threshold, donothing),
    cmocka_unit_test_teardown(test_large_reupload, donothing),
    cmocka_unit_test_teardown(test_verify_upload, teardown),
    cmocka_unit_test_teardown(test_large_put_threshold, teardown),
    cmocka_unit_test_teardown(test_simple_put_uploadfail, teardown),
    cmocka_unit_test_teardown(test_simple_put_use_dev_urandom, teardown),
    cmocka_unit_test_teardown(test_simple_put_create_subfolder, teardown),
    cmocka_unit_test_teardown(test_simple_put_use_s3_regionalURL, teardown),
    cmocka_unit_test_teardown(test_server_side_encryption, donothing),
    cmocka_unit_test_teardown(test_2GBlarge_put, donothing),
    cmocka_unit_test_teardown(test_2GBlarge_get, teardown),
    cmocka_unit_test_teardown(test_simple_put_with_proxy, teardown),
    cmocka_unit_test_teardown(test_simple_put_with_noproxy, teardown),
    cmocka_unit_test_teardown(test_simple_put_with_proxy_fromenv, teardown),
    cmocka_unit_test_teardown(test_simple_put_with_noproxy_fromenv, teardown),
    cmocka_unit_test_teardown(test_upload_file_to_stage_using_stream, donothing),
    cmocka_unit_test_teardown(test_put_get_with_unicode, teardown),
    cmocka_unit_test_teardown(test_simple_put_auto_compress_native, teardown),
    cmocka_unit_test_teardown(test_simple_put_config_temp_dir_native, teardown),
    cmocka_unit_test_teardown(test_simple_get_native, teardown),
    cmocka_unit_test_teardown(test_large_put_auto_compress_native, donothing),
    cmocka_unit_test_teardown(test_large_get_native, donothing),
    cmocka_unit_test_teardown(test_large_get_threshold_native, donothing),
    cmocka_unit_test_teardown(test_large_reupload_native, donothing),
    cmocka_unit_test_teardown(test_verify_upload, teardown),
    cmocka_unit_test_teardown(test_simple_put_use_dev_urandom_native, teardown),
    cmocka_unit_test_teardown(test_simple_put_use_s3_regionalURL_native, teardown),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
  return ret;
}
