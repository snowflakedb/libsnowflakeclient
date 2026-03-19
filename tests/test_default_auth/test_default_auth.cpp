#include "../wiremock/wiremock.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include "snowflake/client.h"
#include "snowflake/secure_storage.h"
#include "../utils/test_setup.h"
#include "../utils/TestSetup.hpp"

using namespace Snowflake::Client;

#define ACCOUNT "test_account"
#define HOST wiremockHost
#define USER "test_user"

static const char* CACHE_DIR = "sf_default_auth_cache";

static int group_setup(void**)
{
    sf_setenv("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", CACHE_DIR);
    rmdir(CACHE_DIR);
    mkdir(CACHE_DIR, 0700);
    return 0;
}

static int group_teardown(void**)
{
    rmdir(CACHE_DIR);
    return 0;
}

void test_default_auth_does_not_cache_mfa_token(void** unused)
{
    SF_UNUSED(unused);

    secure_storage_ptr ss = secure_storage_init();
    secure_storage_remove_credential(ss, HOST, USER, MFA_TOKEN);
    secure_storage_term(ss);

    WiremockRunner* wm = new WiremockRunner("login_response.json", {});

    snowflake_global_set_attribute(SF_GLOBAL_DISABLE_VERIFY_PEER, &SF_BOOLEAN_TRUE);
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_FALSE);

    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, ACCOUNT);
    snowflake_set_attribute(sf, SF_CON_USER, USER);
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, HOST);
    snowflake_set_attribute(sf, SF_CON_PORT, wiremockPort);

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    snowflake_term(sf);
    delete wm;

    ss = secure_storage_init();
    char* cached_token = secure_storage_get_credential(ss, HOST, USER, MFA_TOKEN);
    assert_null(cached_token);
    secure_storage_term(ss);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_default_auth_does_not_cache_mfa_token),
    };
    int ret = cmocka_run_group_tests(tests, group_setup, group_teardown);
    snowflake_global_term();
    return ret;
}
