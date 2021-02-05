/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */


#include "utils/test_setup.h"
#ifndef _WIN32
#include <unistd.h>
#endif

/**
 * Tests converting a string representation of log level to the log level enum
 */
void test_log_str_to_level(void **unused) {
    assert_int_equal(log_from_str_to_level("TRACE"), SF_LOG_TRACE);
    assert_int_equal(log_from_str_to_level("DEBUG"), SF_LOG_DEBUG);
    assert_int_equal(log_from_str_to_level("INFO"), SF_LOG_INFO);
    assert_int_equal(log_from_str_to_level("wArN"), SF_LOG_WARN);
    assert_int_equal(log_from_str_to_level("erroR"), SF_LOG_ERROR);
    assert_int_equal(log_from_str_to_level("fatal"), SF_LOG_FATAL);

    /* negative */
    assert_int_equal(log_from_str_to_level("hahahaha"), SF_LOG_FATAL);
    assert_int_equal(log_from_str_to_level(NULL), SF_LOG_FATAL);
}

#ifndef _WIN32
/**
 * Tests timing of log file creation
 */
void test_log_creation(void **unused) {
    char logname[] = "dummy.log";

    // ensure the log file doesn't exist at the beginning
    remove(logname);
    assert_int_not_equal(access(logname, F_OK), 0);

    log_set_lock(NULL);
    log_set_level(SF_LOG_WARN);
    log_set_quiet(1);
    log_set_path(logname);

    // info log won't trigger the log file creation since log level is set to warning
    log_info("dummy info log");
    assert_int_not_equal(access(logname, F_OK), 0);

    // warning log will trigger the log file creation
    log_warn("dummy warning log");
    assert_int_equal(access(logname, F_OK), 0);

    remove(logname);

/**
 * Tests masking secret information in log
 */
void test_mask_secret_log(void **unused) {
    FILE* fp = fopen("dummy.log", "w+");
    assert_non_null(fp);
    log_set_lock(NULL);
    log_set_level(SF_LOG_TRACE);
    log_set_quiet(1);
    log_set_fp(fp);

    const char * logtext[][2] = {
        {//0
            "Secure log record!",
            "Secure log record!"
        },
        {//1
            "Token =ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ",
            "Token =****"
        },
        {//2
            "idToken : ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ",
            "idToken : ****"
        },
        {//3
            "sessionToken:ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ",
            "sessionToken:****"
        },
        {//4
            "masterToken : 'ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ'",
            "masterToken : '****'"
        },
        {//5
            "assertion content:\"ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ\"",
            "assertion content:\"****\""
        },
        {//6
            "password: random!TEST/-pwd=123++#",
            "password: ****"
        },
        {//7
            "pwd =\"random!TEST/-pwd=123++#",
            "pwd =\"****"
        },
        {//8
            "AWSAccessKeyId=ABCD%efg+1234/567",
            "AWSAccessKeyId=****"
        },
        {//9
            "https://sfc-fake.s3.fakeamazon.com/012345xx-012x-012x-0123-1a2b3c4d/fake/data_fake?x-amz-server-side-encryption-customer-algorithm=fakealgo&response-content-encoding=fakezip&AWSAccessKeyId=ABCD%efg+1234/567&Expires=123456789&Signature=ABCD%efg+1234/567ABCD%efg+1234/567",
            "https://sfc-fake.s3.fakeamazon.com/012345xx-012x-012x-0123-1a2b3c4d/fake/data_fake?x-amz-server-side-encryption-customer-algorithm=fakealgo&response-content-encoding=fakezip&AWSAccessKeyId=****&Expires=123456789&Signature=****"
        },
        {//10
            "aws_key_id='afhl124lomsafho0582'",
            "aws_key_id='****'"
        },
        {//11
            "aws_secret_key = 'dfhuwaojm753omsdfh30oi+fj'",
            "aws_secret_key = '****'"
        },
        {//12
            "\"privateKeyData\": \"abcdefghijk\"",
            "\"privateKeyData\": \"XXXX\""
        },
    };

    char * line = NULL;
    size_t len = 0;
    for (int i = 0; i < 13; i++)
    {
        fseek(fp, 0, SEEK_SET);
        log_trace("%s", logtext[i][0]);
        fseek(fp, 0, SEEK_SET);
        getline(&line, &len, fp);
        if (i != 0)
        {
            assert_null(strstr(line, logtext[i][0]));
        }
        assert_non_null(strstr(line, logtext[i][1]));
    }

    free(line);
    fclose(fp);
}
#endif

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_log_str_to_level),
#ifndef _WIN32
        cmocka_unit_test(test_log_creation),
        cmocka_unit_test(test_mask_secret_log),
#endif
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
