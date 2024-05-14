/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include "snowflake/SFURL.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

using namespace Snowflake::Client;

void test_parse_basic(void ** unused)
{
  SFURL url;

  // Only scheme and host
  url = SFURL::parse("https://snowflake.com");
  assert_string_equal(url.scheme().c_str(), "https");
  assert_string_equal(url.host().c_str(), "snowflake.com");

  // Only scheme and path
  url = SFURL::parse("http:///request_path");
  assert_string_equal(url.scheme().c_str(), "http");
  assert_string_equal(url.host().c_str(), "");
  assert_string_equal(url.path().c_str(), "/request_path");

  // Test with both host and path
  url = SFURL::parse("http://snowflake.com/request_path");
  assert_string_equal(url.scheme().c_str(), "http");
  assert_string_equal(url.host().c_str(), "snowflake.com");
  assert_string_equal(url.path().c_str(), "/request_path");

  // Test with fraction
  url = SFURL::parse("http://user@snowflake.com:8080/request_path#frag");
  assert_string_equal(url.scheme().c_str(), "http");
  assert_string_equal(url.userInfo().c_str(), "user");
  assert_string_equal(url.host().c_str(), "snowflake.com");
  assert_string_equal(url.path().c_str(), "/request_path");
  assert_string_equal(url.port().c_str(), "8080");
  assert_string_equal(url.fragment().c_str(), "frag");

  // Test with parameters
  url = SFURL::parse("http://user@snowflake.com:8080/request_path?key1=val1&key2=val2#frag");
  assert_string_equal(url.scheme().c_str(), "http");
  assert_string_equal(url.userInfo().c_str(), "user");
  assert_string_equal(url.host().c_str(), "snowflake.com");
  assert_string_equal(url.path().c_str(), "/request_path");
  assert_string_equal(url.port().c_str(), "8080");
  assert_string_equal(url.fragment().c_str(), "frag");
  assert_string_equal(url.getQueryParam("key1").c_str(), "val1");
  assert_string_equal(url.getQueryParam("key2").c_str(), "val2");

  // Test with parameters without fragment
  url = SFURL::parse("http://user@snowflake.com:8080/request_path?key1=val1&key2=val2");
  assert_string_equal(url.scheme().c_str(), "http");
  assert_string_equal(url.userInfo().c_str(), "user");
  assert_string_equal(url.host().c_str(), "snowflake.com");
  assert_string_equal(url.path().c_str(), "/request_path");
  assert_string_equal(url.port().c_str(), "8080");
  assert_string_equal(url.fragment().c_str(), "");
  assert_string_equal(url.getQueryParam("key1").c_str(), "val1");
  assert_string_equal(url.getQueryParam("key2").c_str(), "val2");
}

void test_parse_authority(void ** unused)
{
  SFURL url;

   // Test authority with host and port
  url = SFURL::parse("http://snowflake.com:8080/request_path");
  assert_string_equal(url.scheme().c_str(), "http");
  assert_string_equal(url.userInfo().c_str(), "");
  assert_string_equal(url.host().c_str(), "snowflake.com");
  assert_string_equal(url.path().c_str(), "/request_path");
  assert_string_equal(url.port().c_str(), "8080");

  // Test authority with user info, host and port
  url = SFURL::parse("http://user@snowflake.com:8080/request_path");
  assert_string_equal(url.scheme().c_str(), "http");
  assert_string_equal(url.userInfo().c_str(), "user");
  assert_string_equal(url.host().c_str(), "snowflake.com");
  assert_string_equal(url.path().c_str(), "/request_path");
  assert_string_equal(url.port().c_str(), "8080");

  // Test authority with user info and port
  url = SFURL::parse("http://user@snowflake.com/request_path");
  assert_string_equal(url.scheme().c_str(), "http");
  assert_string_equal(url.userInfo().c_str(), "user");
  assert_string_equal(url.host().c_str(), "snowflake.com");
  assert_string_equal(url.path().c_str(), "/request_path");
  assert_string_equal(url.port().c_str(), "");
}

void test_construct(void ** unused)
{
  SFURL url;
  url.scheme("ftp").userInfo("u").host("github.com").port("8082").path("/path/to/test").fragment("frag");
  url.addQueryParam("key1", "val1").addQueryParam("key2", "val2").addQueryParam("key3", "val3");

  assert_string_equal(url.toString().c_str(), "ftp://u@github.com:8082/path/to/test?key1=val1&key2=val2&key3=val3#frag");

  url.userInfo("").port("");
  assert_string_equal(url.toString().c_str(), "ftp://github.com/path/to/test?key1=val1&key2=val2&key3=val3#frag");

  url.port(1024);
  assert_string_equal(url.toString().c_str(), "ftp://github.com:1024/path/to/test?key1=val1&key2=val2&key3=val3#frag");

  url.port("");
  // Trying to renew a query param key does not exist should have no side effect;
  url.renewQueryParam("key4", "val4");
  assert_string_equal(url.toString().c_str(), "ftp://github.com/path/to/test?key1=val1&key2=val2&key3=val3#frag");

  // Renew an existing query param of same length;
  url.renewQueryParam("key2", "val4");
  assert_string_equal(url.toString().c_str(), "ftp://github.com/path/to/test?key1=val1&key2=val4&key3=val3#frag");

  // Renew an existing query param of different length;
  url.renewQueryParam("key2", "valSnow");
  assert_string_equal(url.toString().c_str(), "ftp://github.com/path/to/test?key1=val1&key2=valSnow&key3=val3#frag");
}

#define REQUIRE_THROWS(test_code)     \
{ bool has_throws = false;            \
  try { test_code;}                   \
  catch (...) { has_throws = true; }  \
  assert_true(has_throws);            \
}

void test_error_parse(void ** unused)
{
  // Invalid scheme format
  REQUIRE_THROWS(SFURL::parse("ftp:/github.com"));
  REQUIRE_THROWS(SFURL::parse("ftp:github.com"));

  // Empty param value
  REQUIRE_THROWS(SFURL::parse("http://github.com?key"));
  REQUIRE_THROWS(SFURL::parse("http://github.com?key1=val1&key2"));

  // None numeric port
  REQUIRE_THROWS(SFURL::parse("http://github.com:80ad"));
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_parse_basic),
    cmocka_unit_test(test_parse_authority),
    cmocka_unit_test(test_construct),
    cmocka_unit_test(test_error_parse),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}
