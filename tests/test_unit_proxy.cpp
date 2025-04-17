#include <cassert>
#include "snowflake/Proxy.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

typedef ::Snowflake::Client::Util::Proxy Proxy;

void test_proxy_parts_equality(
    const char *proxy_str,
    const char *user,
    const char *pwd,
    const char *machine,
    unsigned port,
    Proxy::Protocol scheme,
    const char *noProxy,
    bool setProxyFromEnv)
{
    Proxy proxy(proxy_str);

    if (setProxyFromEnv)
    {
        proxy.setProxyFromEnv();
        assert_string_equal(proxy.getNoProxy().c_str(), noProxy);
    }

    assert_string_equal(proxy.getUser().c_str(), user);
    assert_string_equal(proxy.getPwd().c_str(), pwd);
    assert_string_equal(proxy.getMachine().c_str(), machine);
    assert_int_equal(proxy.getPort(), port);
    assert(proxy.getScheme() == scheme);
}

void test_proxy_machine_only(void **unused)
{
    test_proxy_parts_equality("localhost", "", "", "localhost", 80, Proxy::Protocol::HTTP, "", false);
}

void test_proxy_machine_and_port(void **unused)
{
    test_proxy_parts_equality("localhost:8081", "", "", "localhost", 8081, Proxy::Protocol::HTTP, "", false);
}

void test_proxy_machine_and_scheme(void **unused)
{
    test_proxy_parts_equality("https://localhost", "", "", "localhost", 443, Proxy::Protocol::HTTPS, "", false);
}

void test_proxy_machine_port_scheme(void **unused)
{
    test_proxy_parts_equality("http://localhost:5050", "", "", "localhost", 5050, Proxy::Protocol::HTTP, "", false);
}

void test_proxy_all(void **unused)
{
    test_proxy_parts_equality("https://someuser:somepwd@somewhere.com:5050", "someuser", "somepwd", "somewhere.com", 5050, Proxy::Protocol::HTTPS, "", false);
    test_proxy_parts_equality("http://username:password@proxyserver.company.com:80", "username", "password", "proxyserver.company.com", 80, Proxy::Protocol::HTTP, "", false);
}

void test_proxy_empty(void **unused)
{
    test_proxy_parts_equality("", "", "", "", 0, Proxy::Protocol::NONE, "", false);
}

void test_allproxy_noproxy_fromenv(void **unused)
{
    sf_setenv("all_proxy", "https://someuser:somepwd@somewhere.com:5050");
    sf_setenv("no_proxy", "proxyserver.com");
    test_proxy_parts_equality("", "someuser", "somepwd", "somewhere.com", 5050, Proxy::Protocol::HTTPS, "proxyserver.com", true);
    sf_unsetenv("all_proxy");
    sf_unsetenv("no_proxy");
}

void test_httpsproxy_fromenv(void **unused)
{
    sf_setenv("https_proxy", "https://someuser:somepwd@somewhere.com:5050");
    test_proxy_parts_equality("", "someuser", "somepwd", "somewhere.com", 5050, Proxy::Protocol::HTTPS, "", true);
    sf_unsetenv("https_proxy");
}

void test_httpproxy_fromenv(void **unused)
{
    sf_setenv("http_proxy", "http://username:password@proxyserver.company.com:80");
    test_proxy_parts_equality("", "username", "password", "proxyserver.company.com", 80, Proxy::Protocol::HTTP, "", true);
    sf_unsetenv("http_proxy");
}

void test_noproxy_fromenv(void **unused)
{
    sf_setenv("NO_PROXY", "proxyserver.company.com");
    test_proxy_parts_equality("", "", "", "", 0, Proxy::Protocol::NONE, "proxyserver.company.com", true);
    sf_unsetenv("NO_PROXY");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_proxy_machine_only),
        cmocka_unit_test(test_proxy_machine_and_port),
        cmocka_unit_test(test_proxy_machine_and_scheme),
        cmocka_unit_test(test_proxy_machine_port_scheme),
        cmocka_unit_test(test_proxy_all),
        cmocka_unit_test(test_proxy_empty),
        cmocka_unit_test(test_allproxy_noproxy_fromenv),
        cmocka_unit_test(test_httpsproxy_fromenv),
        cmocka_unit_test(test_httpproxy_fromenv),
        cmocka_unit_test(test_noproxy_fromenv)};
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    return ret;
}
