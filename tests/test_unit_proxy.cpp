/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <cassert>
#include "util/Proxy.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

typedef ::Snowflake::Client::Util::Proxy Proxy;

void test_proxy_parts_equality(const char *proxy_str, const char *user, const char *pwd, const char *machine, unsigned port, Proxy::Protocol scheme) {
    Proxy proxy(proxy_str);

    assert_string_equal(proxy.getUser().c_str(), user);
    assert_string_equal(proxy.getPwd().c_str(), pwd);
    assert_string_equal(proxy.getMachine().c_str(), machine);
    assert_int_equal(proxy.getPort(), port);
    assert(proxy.getScheme() == scheme);
}

void test_proxy_machine_only(void **unused)
{
    test_proxy_parts_equality("localhost", "", "", "localhost", 80, Proxy::Protocol::HTTP);
}

void test_proxy_machine_and_port(void **unused)
{
    test_proxy_parts_equality("localhost:8081", "", "", "localhost", 8081, Proxy::Protocol::HTTP);
}

void test_proxy_machine_and_scheme(void **unused)
{
    test_proxy_parts_equality("https://localhost", "", "", "localhost", 443, Proxy::Protocol::HTTPS);
}

void test_proxy_machine_port_scheme(void **unused)
{
    test_proxy_parts_equality("http://localhost:5050", "", "", "localhost", 5050, Proxy::Protocol::HTTP);
}

void test_proxy_all(void **unused)
{
    test_proxy_parts_equality("https://someuser:somepwd@somewhere.com:5050", "someuser", "somepwd", "somewhere.com", 5050, Proxy::Protocol::HTTPS);
    test_proxy_parts_equality("http://username:password@proxyserver.company.com:80", "username", "password", "proxyserver.company.com", 80, Proxy::Protocol::HTTP);
}

void test_proxy_empty(void **unused)
{
    test_proxy_parts_equality("", "", "", "", 0, Proxy::Protocol::NONE);
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
