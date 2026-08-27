#include "../wiremock/wiremock.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include "snowflake/client.h"
#include "snowflake/secure_storage.h"
#include "../utils/test_setup.h"
#include "../utils/TestSetup.hpp"

using namespace Snowflake::Client;

static int group_setup(void**)
{
    return 0;
}

static int group_teardown(void**)
{
    return 0;
}

void test_retry_parts_uploading(void** unused)
{
    SF_UNUSED(unused);

}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_retry_parts_uploading),
    };
    int ret = cmocka_run_group_tests(tests, group_setup, group_teardown);
    snowflake_global_term();
    return ret;
}
