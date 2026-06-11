#include <string>
#include <vector>
#include "util/SnowflakeCommon.hpp"
#include "utils/TestSetup.hpp"
#include "utils/test_setup.h"

using Snowflake::Client::Util::isUnsafeDownloadFileName;

/**
 * Tests for isUnsafeDownloadFileName(): the local destination file name for a
 * GET download must be a single path component, so names with separators or
 * directory references are rejected while ordinary file names are accepted.
 */

/**
 * Plain, single-component file names must always be accepted, including names
 * with dots, spaces, dashes, non-ASCII bytes and unusual but valid characters.
 */
void test_download_filename_valid_names(void **unused)
{
  const std::vector<std::string> validNames = {
    "file.csv",
    "file1.csv.gz",
    "some-file_name.123.parquet",
    "UPPER.TXT",
    ".bashrc",                       // leading single dot (hidden file) is fine
    "..hidden",                      // starts with .. but is not exactly ".."
    "hidden..",                      // ends with .. but is not exactly ".."
    "a..b",                          // .. in the middle
    "..hidden..csv",
    "...",                           // not "." or ".." and contains no separator
    "....",
    "file with spaces.csv",
    " leading-space.csv",
    "trailing-space.csv ",
    "-rf",                           // looks like a flag, but is a valid name
    "--flag.txt",
    "name+plus%percent~tilde.txt",
    "file.tar.gz",
    "caf\xC3\xA9.txt",               // UTF-8 "café.txt"
    "\xD1\x84\xD0\xB0\xD0\xB9\xD0\xBB.csv", // UTF-8 Cyrillic "файл.csv"
    std::string(250, 'a') + ".csv",  // long single component
  };

  for (const auto &name : validNames)
  {
    assert_false(isUnsafeDownloadFileName(name));
  }
}

/**
 * Empty names and current/parent directory references must be rejected on
 * every platform.
 */
void test_download_filename_dot_and_empty(void **unused)
{
  assert_true(isUnsafeDownloadFileName(""));
  assert_true(isUnsafeDownloadFileName("."));
  assert_true(isUnsafeDownloadFileName(".."));
}

/**
 * Embedded NUL bytes must be rejected wherever they appear, otherwise the
 * bytes after the NUL would be dropped by downstream C-string handling and the
 * validated name would differ from the opened name.
 */
void test_download_filename_embedded_nul(void **unused)
{
  assert_true(isUnsafeDownloadFileName(std::string("ab\0junk", 7)));   // middle
  assert_true(isUnsafeDownloadFileName(std::string("\0abc", 4)));      // leading
  assert_true(isUnsafeDownloadFileName(std::string("abc\0", 4)));      // trailing
  assert_true(isUnsafeDownloadFileName(std::string("file\0.csv", 9)));
  assert_true(isUnsafeDownloadFileName(std::string("a\0/b", 4)));
  assert_true(isUnsafeDownloadFileName(std::string("\0", 1)));         // lone NUL
}

/**
 * A forward slash is a directory separator on all platforms, so any name
 * containing one (leading, trailing, middle, repeated) must be rejected
 * regardless of OS.
 */
void test_download_filename_forward_slash(void **unused)
{
  const std::vector<std::string> rejectedNames = {
    "/",
    "a/",                            // trailing
    "/a",                            // leading
    "a/b",                           // middle
    "a//b",                          // repeated
    "sub/dir/file.csv",
    "./a",
    "../a",
    "../../a/b.csv",
    "a/../b",
    "/absolute/path/file",
  };

  for (const auto &name : rejectedNames)
  {
    assert_true(isUnsafeDownloadFileName(name));
  }
}

/**
 * On Windows the backslash is a path separator and ':' introduces a drive /
 * alternate data stream specifier, so names containing them are rejected
 * there. On POSIX both are legal file name bytes and are accepted.
 */
void test_download_filename_backslash_and_drive(void **unused)
{
  const std::vector<std::string> windowsRejectedNames = {
    "\\",                            // lone backslash
    "a\\",                           // trailing
    "\\a",                           // leading
    "a\\b",                          // middle
    "a\\\\b",                        // repeated
    "..\\a.csv",
    "..\\..\\a\\b.csv",
    "sub\\dir\\file.csv",
    "C:",                            // bare drive
    "C:\\",
    "C:\\a\\b.csv",
    "C:relative",                    // drive-relative
    "name.csv:stream",               // alternate data stream
    "a/b\\c",                        // mixed (also has '/', see note below)
  };

  for (const auto &name : windowsRejectedNames)
  {
#ifdef _WIN32
    assert_true(isUnsafeDownloadFileName(name));
#else
    // On POSIX '\\' and ':' are ordinary bytes. The only entry that is also
    // rejected on POSIX is the one containing a forward slash.
    if (name.find('/') != std::string::npos)
    {
      assert_true(isUnsafeDownloadFileName(name));
    }
    else
    {
      assert_false(isUnsafeDownloadFileName(name));
    }
#endif
  }
}

/**
 * A forward slash is rejected on every platform even when combined with
 * characters that are only separators on Windows.
 */
void test_download_filename_mixed_separators(void **unused)
{
  // Contains '/', so unsafe everywhere.
  assert_true(isUnsafeDownloadFileName("a/b\\c"));
  assert_true(isUnsafeDownloadFileName("\\a/b"));
  assert_true(isUnsafeDownloadFileName("/a\\b"));
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_download_filename_valid_names),
    cmocka_unit_test(test_download_filename_dot_and_empty),
    cmocka_unit_test(test_download_filename_embedded_nul),
    cmocka_unit_test(test_download_filename_forward_slash),
    cmocka_unit_test(test_download_filename_backslash_and_drive),
    cmocka_unit_test(test_download_filename_mixed_separators),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
