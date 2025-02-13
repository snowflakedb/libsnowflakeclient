/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "../lib/snowflake_util.h"

void test_cJSON_to_PicoJson(void** unused) 
{
    //cJSON* data = snowflake_cJSON_CreateObject();
    //cJSON* elements = snowflake_cJSON_CreateObject();
    //snowflake_cJSON_AddStringToObject(elements, "Authenticator", "test_authenticator");
    //snowflake_cJSON_AddStringToObject(elements, "key", "test_key");
    //snowflake_cJSON_AddItemToObject(data, "data", elements);

    //jsonObject_t picojson;
    //cJSONtoPicoJson(data, picojson);
    //jsonObject_t& picoData = picojson["data"].get<jsonObject_t>();
    //assert_string_equal(picoData["Authenticator"].get<std::string>().c_str(), "test_authenticator");
    //assert_string_equal(picoData["key"].get<std::string>().c_str(), "test_key");
}


int main(void) {
 
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cJSON_to_PicoJson),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
