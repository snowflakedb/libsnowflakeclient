/*
 * Copyright (c) 2023 Snowflake Computing, Inc. All rights reserved.
 */

#include <cassert>
#include <string>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "memory.h"
#include <vector>

typedef struct sf_string_attributes {
    SF_ATTRIBUTE type;
    std::string value;
} sf_string_attributes;

std::vector<sf_string_attributes> strAttributes = {
    { SF_CON_ACCOUNT, "test_account" },
    { SF_CON_REGION, "test_region" },
    { SF_CON_USER, "test_user" },
    { SF_CON_PASSWORD, "test_password" },
    { SF_CON_DATABASE, "test_database" },
    { SF_CON_SCHEMA, "test_schema" },
    { SF_CON_WAREHOUSE, "test_warehouse" },
    { SF_CON_ROLE, "test_role" },
    { SF_CON_HOST, "test_host" },
    { SF_CON_PORT, "test_port" },
    { SF_CON_PROTOCOL, "test_protocol" },
    { SF_CON_PASSCODE, "test_passcode" },
    { SF_CON_APPLICATION_NAME, "test_application_name" },
    { SF_CON_APPLICATION_VERSION, "test_application_version" },
    { SF_CON_AUTHENTICATOR, "test_authenticator" },
    { SF_CON_TIMEZONE, "test_timezone" },
    { SF_CON_SERVICE_NAME, "test_service_name" },
    { SF_CON_APPLICATION, "test_application" },
    { SF_CON_PRIV_KEY_FILE, "test_priv_key_file" },
    { SF_CON_PRIV_KEY_FILE_PWD, "test_priv_key_file_pwd" },
    { SF_CON_PROXY, "test_proxy" },
    { SF_CON_NO_PROXY, "test_no_proxy" },
    { SF_DIR_QUERY_URL, "test_dir_query_url" },
    { SF_DIR_QUERY_URL_PARAM, "test_dir_query_url_param" },
    { SF_DIR_QUERY_TOKEN, "test_dir_query_token" },
    { SF_QUERY_RESULT_TYPE, "test_query_result_type" },
};

typedef struct sf_bool_attributes {
    SF_ATTRIBUTE type;
    bool value;
} sf_bool_attributes;

std::vector<sf_bool_attributes> boolAttributes = {
    { SF_CON_PASSCODE_IN_PASSWORD, true },
    { SF_CON_INSECURE_MODE, false },
    { SF_CON_AUTOCOMMIT, true },
};

typedef struct sf_int_attributes {
    SF_ATTRIBUTE type;
    int64 value;
} sf_int_attributes;

std::vector<sf_int_attributes> intAttributes = {
    { SF_CON_LOGIN_TIMEOUT, 123 },
    { SF_CON_NETWORK_TIMEOUT, 456 },
    { SF_CON_JWT_TIMEOUT, 789 },
    { SF_CON_JWT_CNXN_WAIT_TIME, 987 },
    { SF_CON_MAX_CON_RETRY, 6 },
    { SF_RETRY_ON_CURLE_COULDNT_CONNECT_COUNT, 5 },
    { SF_CON_RETRY_TIMEOUT, 456 },
    { SF_CON_MAX_RETRY, 8 },
    { SF_CON_RETRY_TIMEOUT, 123 },
    { SF_CON_MAX_RETRY, 6 },
    { SF_CON_RETRY_TIMEOUT, 0 },
    { SF_CON_MAX_RETRY, 0 },
    { SF_CON_MAX_VARCHAR_SIZE, SF_DEFAULT_MAX_OBJECT_SIZE },
    { SF_CON_MAX_BINARY_SIZE, SF_DEFAULT_MAX_OBJECT_SIZE / 2 },
    { SF_CON_MAX_VARIANT_SIZE, SF_DEFAULT_MAX_OBJECT_SIZE },
};

// unit test for snowflake_set_attribute and snowflake_get_attribute for all SF_ATTRIBUTE
void test_set_get_all_attributes(void **unused)
{
    SF_CONNECT *sf = snowflake_init();

    // Connection parameters that cannot be set by user
    sf->service_name = (char*)"test_service_name";
    sf->query_result_format = (char*)"test_query_result_type";

    SF_STATUS status = SF_STATUS_EOF;
    void* value = NULL;

    // set and get string attributes
    for (sf_string_attributes attr : strAttributes)
    {
        if (attr.type == SF_CON_SERVICE_NAME ||
            attr.type == SF_QUERY_RESULT_TYPE)
        {
            status = snowflake_get_attribute(sf, attr.type, &value);
            if (status != SF_STATUS_SUCCESS)
            {
                dump_error(&(sf->error));
            }
            assert_int_equal(status, SF_STATUS_SUCCESS);
            assert_string_equal(value, attr.value.c_str());
        }
        else
        {
            status = snowflake_set_attribute(sf, attr.type, attr.value.c_str());
            if (status != SF_STATUS_SUCCESS)
            {
                dump_error(&(sf->error));
            }
            assert_int_equal(status, SF_STATUS_SUCCESS);

            status = snowflake_get_attribute(sf, attr.type, &value);
            if (status != SF_STATUS_SUCCESS)
            {
                dump_error(&(sf->error));
            }
            assert_int_equal(status, SF_STATUS_SUCCESS);
            assert_string_equal(value, attr.value.c_str());
        }

        if (value)
        {
            value = NULL;
        }
    }

    // set and get bool attributes
    for (sf_bool_attributes attr : boolAttributes)
    {
        status = snowflake_set_attribute(sf, attr.type, &attr.value);
        if (status != SF_STATUS_SUCCESS)
        {
            dump_error(&(sf->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        status = snowflake_get_attribute(sf, attr.type, &value);
        if (status != SF_STATUS_SUCCESS)
        {
            dump_error(&(sf->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert_true(*((sf_bool *) value) == attr.value);

        if (value)
        {
            value = NULL;
        }
    }

    // set and get int attributes
    for (sf_int_attributes attr : intAttributes)
    {
        if ((attr.type != SF_CON_MAX_VARCHAR_SIZE) &&
            (attr.type != SF_CON_MAX_BINARY_SIZE) &&
            (attr.type != SF_CON_MAX_VARIANT_SIZE))
        {
            status = snowflake_set_attribute(sf, attr.type, &attr.value);
            if (status != SF_STATUS_SUCCESS)
            {
                dump_error(&(sf->error));
            }
            assert_int_equal(status, SF_STATUS_SUCCESS);
        }

        status = snowflake_get_attribute(sf, attr.type, &value);
        if (status != SF_STATUS_SUCCESS)
        {
            dump_error(&(sf->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // for SF_CON_MAX_CON_RETRY and SF_CON_LOGIN_TIMEOUT application
        // can only increase the default value.
        if (attr.type == SF_CON_MAX_CON_RETRY ||
            attr.type == SF_CON_MAX_RETRY ||
            attr.type == SF_RETRY_ON_CURLE_COULDNT_CONNECT_COUNT)
        {
            if ((attr.type == SF_CON_MAX_RETRY) && (attr.value < 7) && (attr.value != 0))
            {
                assert_int_equal(*((int8 *)value), 7);
            }
            else
            {
                assert_int_equal(*((int8 *)value), attr.value);
            }
        }
        else
        {
            if ((attr.type == SF_CON_RETRY_TIMEOUT) && (attr.value < 300) && (attr.value != 0))
            {
                assert_int_equal(*((int64 *)value), 300);
            }
            else
            {
                assert_int_equal(*((int64 *)value), attr.value);
            }
        }

        if (value)
        {
            value = NULL;
        }
    }
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_set_get_all_attributes),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
