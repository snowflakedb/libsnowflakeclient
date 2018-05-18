/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <fstream>
#include <FileMetadataInitializer.hpp>
#include "FileMetadata.hpp"
#include "FileCompressionType.hpp"
#include "utils/test_setup.h"
#include <unordered_set>
#include <iostream>
#include <map>

#define FILES_IN_DIR "file1.csv", "file2.csv", "file3.csv", "file4.csv", "file1.gz"


typedef ::Snowflake::Client::FileCompressionType FileCompressionType;

void getDataDirectory(std::string& dataDir)
{
  const std::string current_file = __FILE__;
  std::string testsDir = current_file.substr(0, current_file.find_last_of('/'));
  dataDir = testsDir + "/data/";
}

void test_file_pattern_match_core(std::vector<std::string> *expectedFiles,
                                  const char *filePattern)
{
  std::vector<FileMetadata> smallFileMetadata;
  std::vector<FileMetadata> largeFileMetadata;

  FileMetadataInitializer initializer(&smallFileMetadata, &largeFileMetadata);
  initializer.setSourceCompression((char *)"none");


  std::string srcLocation;
  getDataDirectory(srcLocation);
  srcLocation += "test_file_match_dir/";

  std::string fullFilePattern = srcLocation + filePattern;
  initializer.populateSrcLocUploadMetadata(fullFilePattern);

  std::unordered_set<std::string> actualFiles;
  for (auto i = smallFileMetadata.begin(); i != smallFileMetadata.end(); i++)
  {
    actualFiles.insert(i->srcFileName);
  }

  std::unordered_set<std::string> expectedFilesFull;
  for (auto i = expectedFiles->begin(); i != expectedFiles->end(); i++)
  {
    std::string expectedFileFull = srcLocation + *i;
    expectedFilesFull.insert(expectedFileFull);
  }

  assert_true(expectedFilesFull == actualFiles);
}

static int file_pattern_match_teardown(void ** unused)
{
  std::string srcLocation;
  getDataDirectory(srcLocation);
  srcLocation += "test_file_match_dir/";
  std::string rm = "rm -rf " + srcLocation;

  system(rm.c_str());
  return 0;
}

static int file_pattern_match_setup(void **unused)
{
  std::string srcLocation;
  getDataDirectory(srcLocation);
  srcLocation += "test_file_match_dir/";

  std::string mkdirCmd = "mkdir -p " + srcLocation;
  int ret = system(mkdirCmd.c_str());
  assert_int_equal(0, ret);

  std::vector<std::string> allFiles = {FILES_IN_DIR};
  for (auto i = allFiles.begin(); i != allFiles.end(); i++)
  {
    std::string fullFileName = srcLocation + *i;
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

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test_setup_teardown(test_file_pattern_match,
                                    file_pattern_match_setup,
                                    file_pattern_match_teardown),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
