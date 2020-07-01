/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */


#include <aws/core/Aws.h>
#include "utils/test_setup.h"


void test_include_aws(void **unused) {
    // Very basic test to see if library is properly linked
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    Aws::ShutdownAPI(options);
}

int main(void) {
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_include_aws),
    };
    const char *cloud_provider = std::getenv("CLOUD_PROVIDER");
    if(cloud_provider && (strcmp(cloud_provider,"AWS") == 0) ) {
      std::cout << "Aws Include test running on AWS cloud provider" << std::endl;
      int ret = cmocka_run_group_tests(tests, NULL, NULL);
      return ret;
    }
    std::cout << "Not running aws Include test. Cloud provider is " << cloud_provider << std::endl;
    return 0;
}
