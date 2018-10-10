/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "util/Proxy.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

typedef ::Snowflake::Client::Util::Proxy Proxy;

void test_proxy_parts_equality(const char *proxy_str, const char *user, const char *pwd, const char *machine, unsigned port, const char *scheme) {
    Proxy proxy(proxy_str);

    assert_string_equal(proxy.getUser().c_str(), user);
    assert_string_equal(proxy.getPwd().c_str(), pwd);
    assert_string_equal(proxy.getMachine().c_str(), machine);
    assert_int_equal(proxy.getPort(), port);
    assert_string_equal(proxy.getScheme().c_str(), scheme);
}

void test_proxy_machine_only(void **unused)
{
    test_proxy_parts_equality("localhost", "", "", "localhost", 0, "");
}

void test_proxy_machine_and_port(void **unused)
{
    test_proxy_parts_equality("localhost:8081", "", "", "localhost", 8081, "");
}

void test_proxy_machine_and_scheme(void **unused)
{
    test_proxy_parts_equality("https://localhost", "", "", "localhost", 0, "https");
}

void test_proxy_machine_port_scheme(void **unused)
{
    test_proxy_parts_equality("http://localhost:5050", "", "", "localhost", 5050, "http");
}

void test_proxy_all(void **unused)
{
    test_proxy_parts_equality("https://someuser:somepwd@somewhere.com:5050", "someuser", "somepwd", "somewhere.com", 5050, "https");
}

void test_proxy_empty(void **unused)
{
    test_proxy_parts_equality("", "", "", "", 0, "");
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_proxy_machine_only),
    cmocka_unit_test(test_proxy_machine_and_port),
    cmocka_unit_test(test_proxy_machine_and_scheme),
    cmocka_unit_test(test_proxy_machine_port_scheme),
    cmocka_unit_test(test_proxy_all),
    cmocka_unit_test(test_proxy_empty),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
