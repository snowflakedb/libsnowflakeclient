/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */


#include <aws/core/Aws.h>


int main() {
    // Very basic test to see if library is properly linked
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    Aws::ShutdownAPI(options);
}
