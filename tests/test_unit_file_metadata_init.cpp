/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <fstream>
#include <FileMetadataInitializer.hpp>
#include "FileMetadata.hpp"
#include "FileCompressionType.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "snowflake/platform.h"
#include <unordered_set>
#include <iostream>
#include <map>

#define FILES_IN_DIR "file1.csv", "file2.csv", "file3.csv", "file4.csv", "file1.gz"


typedef ::Snowflake::Client::FileCompressionType FileCompressionType;

using namespace ::Snowflake::Client;

std::string getTestFileMatchDir()
{
  std::string srcLocation = TestSetup::getDataDir();
  srcLocation += "test_file_match_dir"; 
  srcLocation += PATH_SEP;
  return srcLocation;
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

std::vector<std::string> getListOfTestFileMatchDir()
{
  std::vector<std::string> listOfTestDirs; 
  #ifdef _WIN32
  std::string srcLocation = getTestFileMatchDir();
  replaceStrAll(srcLocation,"\\","/");
  // Directory path looks like C:/Users/vreddy/App/Temp/adkf-kasdfk-adsfl/
  // Testing forward slash for put/get support
  listOfTestDirs.push_back(srcLocation);
  #endif
  std::string matchDir = getTestFileMatchDir();
  listOfTestDirs.push_back(matchDir);
  
  return listOfTestDirs;
}

void test_file_pattern_match_core(std::vector<std::string> *expectedFiles,
                                  const char *filePattern)
{
  std::vector<std::string> listTestDir = getListOfTestFileMatchDir();
  for (auto testDir : listTestDir)
  {
    std::vector<FileMetadata> smallFileMetadata;
    std::vector<FileMetadata> largeFileMetadata;

    FileMetadataInitializer initializer(smallFileMetadata, largeFileMetadata);
    initializer.setSourceCompression((char *)"none");

    std::string fullFilePattern = testDir + filePattern;
    initializer.populateSrcLocUploadMetadata(fullFilePattern, DEFAULT_UPLOAD_DATA_SIZE_THRESHOLD);

    std::unordered_set<std::string> actualFiles;
    for (auto i = smallFileMetadata.begin(); i != smallFileMetadata.end(); i++) 
    {
      actualFiles.insert(i->srcFileName);
    }

    std::unordered_set<std::string> expectedFilesFull;
    for (auto i = expectedFiles->begin(); i != expectedFiles->end(); i++) 
    {
      std::string expectedFileFull = testDir + *i;
      expectedFilesFull.insert(expectedFileFull);
    }

    assert_true(expectedFilesFull == actualFiles);
  }
}

static int file_pattern_match_teardown(void ** unused)
{
  std::string testDir = getTestFileMatchDir();
  sf_delete_directory_if_exists(testDir.c_str());
  return 0;
}

static int file_pattern_match_setup(void **unused)
{
  std::string testDir = getTestFileMatchDir();
  int ret = sf_create_directory_if_not_exists(testDir.c_str());
  assert_int_equal(0, ret);

  std::vector<std::string> allFiles = {FILES_IN_DIR};
  for (auto i = allFiles.begin(); i != allFiles.end(); i++)
  {
    std::string fullFileName = testDir + *i;
    std::ofstream ofs(fullFileName);
    assert_true(ofs.is_open());
    ofs.close();
  }
  return 0;
}

void test_file_pattern_match(void **unused)
{
  std::map<std::string, std::vector<std::string>> testcases=
    {
      {"*", {"file1.csv", "file2.csv", "file3.csv", "file4.csv", "file1.gz"}},
      {"*.csv", {"file1.csv", "file2.csv", "file3.csv", "file4.csv"}},
      {"file?.csv", {"file1.csv", "file2.csv", "file3.csv", "file4.csv"}},
      {"file_.csv", {}},
      {"file1.*", {"file1.csv", "file1.gz"}},
    };

  for (auto i = testcases.begin(); i != testcases.end(); i ++)
  {
    test_file_pattern_match_core(&(i->second), i->first.c_str());
  }

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
    cmocka_unit_test_setup_teardown(test_file_pattern_match,
                                    file_pattern_match_setup,
                                    file_pattern_match_teardown),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
  return ret;
}
