#include <aws/core/Aws.h>
#include <snowflake/client.h>
#include <fstream>
#include <stdlib.h>
#include <time.h>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "snowflake/IStatementPutGet.hpp"
#include "snowflake/platform.h"
#include "StatementPutGet.hpp"
#include "FileTransferAgent.hpp"

#define SMALL_FILE_PREFIX "test_small_file_"
#define LARGE_FILE_PREFIX "test_large_file_"

using namespace ::Snowflake::Client;

void populateDataInTestDir(std::string &testDir, int numberOfFiles, int numberOfRows)
{
  int ret = sf_create_directory_if_not_exists(testDir.c_str());
  assert_int_equal(0, ret);

  std::string filePrefix;
  if (numberOfRows == 1)
  {
    filePrefix = SMALL_FILE_PREFIX;
  }
  else
  {
    filePrefix = LARGE_FILE_PREFIX;
  }

  for (int i=0; i<numberOfFiles;i ++)
  {
    std::string fullFileName = testDir + filePrefix +
                               std::to_string(i) + ".csv";
    std::ofstream ofs(fullFileName);
    assert_true(ofs.is_open());
    for (int j=0; j<numberOfRows; j++)
    {
      ofs << "test_string11111,test_string222222,test_string333333" << std::endl;
    }
    ofs.close();
  }
}

void test_parallel_upload_download_core(int fileNumber, int numberOfRows = 1)
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

  std::string create_table("create or replace table test_parallel_upload_download(c1 string"
                             ", c2 string, c3 string)");
  ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  std::string dataDir = TestSetup::getDataDir();
  time_t t;
  srand((unsigned) time(&t));
  std::string randStr = std::string("_") + std::to_string(rand());
  std::string uploadDir = dataDir + "test_parallel_upload" + randStr;
  uploadDir += PATH_SEP;

  populateDataInTestDir(uploadDir, fileNumber, numberOfRows);

  std::string files = uploadDir + "*";
  std::string putCommand = "put file://" + files + " @%test_parallel_upload_download auto_compress=false";

  std::unique_ptr<IStatementPutGet> stmtPutGet = std::unique_ptr
    <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));
  Snowflake::Client::FileTransferAgent agent(stmtPutGet.get());

  ITransferResult *result = agent.execute(&putCommand);
  assert_int_equal(fileNumber, result->getResultSize());

  std::string put_status;
  while(result->next())
  {
    result->getColumnAsString(6, put_status);
    assert_string_equal("UPLOADED", put_status.c_str());
  }

  std::string copyCommand = "copy into test_parallel_upload_download";
  ret = snowflake_query(sfstmt, copyCommand.c_str(), copyCommand.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);

  assert_int_equal(snowflake_num_rows(sfstmt), fileNumber);
  
  const char *c2 = nullptr;
  while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
    snowflake_column_as_const_str(sfstmt, 2, &c2);
    assert_string_equal(c2, "LOADED");
  }
  assert_int_equal(status, SF_STATUS_EOF);

  std::string downloadDir = dataDir + "test_parallel_download" + randStr;
  std::string getCommand = "get @%test_parallel_upload_download file://" + downloadDir;
  result = agent.execute(&getCommand);
  assert_int_equal(fileNumber, result->getResultSize());

  std::string get_status;
  while(result->next())
  {
    result->getColumnAsString(2, get_status);
    assert_string_equal("DOWNLOADED", get_status.c_str());
  }

#ifdef _WIN32
  std::string compCmd = "FC " + uploadDir + "* " + downloadDir + "\\*";
#else
  std::string compCmd = "diff " + uploadDir + " " + downloadDir + "/";
#endif
  int res = system(compCmd.c_str());
  assert_int_equal(0, res);

  snowflake_stmt_term(sfstmt);

  /* close and term */
  snowflake_term(sf); // purge snowflake context

  sf_delete_directory_if_exists(uploadDir.c_str());
  sf_delete_directory_if_exists(downloadDir.c_str());
}

static int teardown(void **unused)
{
  SF_CONNECT *sf = setup_snowflake_connection();
  snowflake_connect(sf);

  SF_STMT *sfstmt = snowflake_stmt(sf);
  std::string rm = "rm @%test_parallel_upload_download";
  snowflake_query(sfstmt, rm.c_str(), rm.size());

  std::string truncate = "drop table if exists test_parallel_upload";
  snowflake_query(sfstmt, truncate.c_str(), truncate.size());

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);

  return 0;
}

void test_small_file_concurrent_upload_download(void **unused)
{
  test_parallel_upload_download_core(10, 50000);
}

void test_large_file_multipart_upload(void **unused)
{
  test_parallel_upload_download_core(1, 500000);
}

void test_large_file_concurrent_upload_download(void **unused)
{
// Jenkins node on Mac has issue with large file.
#ifdef __APPLE__
  char* jobname = getenv("JOB_NAME");
  if (jobname && (strlen(jobname) > 0))
  {
    return;
  }
#endif

  test_parallel_upload_download_core(10, 200000);
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  if(!setup_random_database()) {
    std::cout << "Failed to setup random database, fallback to use regular one." << std::endl;
  }

  return 0;
}

static int gr_teardown(void **unused)
{
  drop_random_database();
  snowflake_global_term();
  return 0;
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
    cmocka_unit_test_teardown(test_small_file_concurrent_upload_download, teardown),
    cmocka_unit_test_teardown(test_large_file_multipart_upload, teardown),
    cmocka_unit_test_teardown(test_large_file_concurrent_upload_download, teardown),
  };
  const char *cloud_provider = std::getenv("CLOUD_PROVIDER");
  if( cloud_provider && ( strcmp(cloud_provider,"AWS") == 0 ) ) {
    std::cout << "AWS parallel_upload_download running on cloud provider " << cloud_provider << std::endl;
    int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
    return ret;
  }
  std::cout << "parallel_upload_download not running on cloud provider " << cloud_provider << std::endl;
  return 0;
}
