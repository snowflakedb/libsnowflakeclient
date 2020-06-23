/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "FileCompressionType.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

typedef ::Snowflake::Client::FileCompressionType FileCompressionType;

void test_file_type_detect_core(const FileCompressionType *expected_type,
                                const char * file_name)
{
  std::string full_file_path = TestSetup::getDataDir();
  full_file_path += file_name;

  const FileCompressionType * actual_type = FileCompressionType
    ::guessCompressionType(full_file_path);

  assert_memory_equal(expected_type, actual_type, sizeof(FileCompressionType));
}

void test_detect_gzip(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::GZIP, "small_file.csv.gz");
}

void test_detect_none(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::NONE, "small_file.csv");
}

void test_detect_bz2(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::BZIP2, "small_file.csv.bz2");
}

void test_detect_zst(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::ZSTD, "small_file.csv.zst");
}

void test_detect_zero(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::NONE, "zero_byte.csv");
}

void test_detect_one(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::NONE, "one_byte.csv");
}

void test_detect_brotli(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::BROTLI, "brotli_sample.txt.br");
}

void test_detect_parquet(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::PARQUET, "nation.impala.parquet");
}

void test_detect_orc(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::ORC, "TestOrcFile.test1.orc");
}

void test_detect_noextension(void **unused)
{
  test_file_type_detect_core(&FileCompressionType::NONE, "small_file");
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_detect_gzip),
    cmocka_unit_test(test_detect_none),
    cmocka_unit_test(test_detect_bz2),
    cmocka_unit_test(test_detect_zst),
    cmocka_unit_test(test_detect_zero),
    cmocka_unit_test(test_detect_one),
    cmocka_unit_test(test_detect_brotli),
    cmocka_unit_test(test_detect_parquet),
    cmocka_unit_test(test_detect_orc),
    cmocka_unit_test(test_detect_noextension),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
