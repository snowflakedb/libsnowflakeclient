
// DON'T DELETE THE FOLLOWING LINE
#define CATCH_CONFIG_MAIN

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include "utils/test_setup.h"
#include "oobtelemetry.h"

std::string getCABundleFile() {
    const char *cabundle_path = getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    if (cabundle_path == nullptr) {
        fprintf(stderr, "Set the environment variable SNOWFLAKE_TEST_CA_BUNDLE_FILE!");
        std::exit(EXIT_FAILURE);
    }
    return std::string(cabundle_path);
}

typedef struct {
    const char *host;
    const char *port;
    const char *account;
    const char *user;
    const char *token;
    const char *authenticator;
    const char *db;
    const char *schema;
    const char *warehouse;
    const char *role;
    const char *ssl;
    const char *deployment; // the deployment OOB was sent to
    const char *rc; // return code
} SF_SETTINGS;

void test_oob(void **) {
    int evnt = 0;

    std::string CABundlePath = getCABundleFile();

    SF_SETTINGS testcase[] = {
            // prod
            {
                    "sfctest0.snowflakecomputing.com",
                    "443",
                    "sfctest0",
                    "snowman",
                    "TOKEN",
                    "snowflake",
                    "TestDB",
                    "TestSchema",
                    "Regress",
                    "AccountAdmin",
                    "1",
                    "prod",
                    "0"
            },
            // prod
            {
                    "sfctest0.east-us-2.azure.snowflakecomputing.com",
                    "443",
                    "sfctest0",
                    "snowman",
                    "TOKEN",
                    "snowflake",
                    "TestDB",
                    "TestSchema",
                    "Regress",
                    "AccountAdmin",
                    "1",
                    "prod",
                    "0"
            },
            // qa1
            {
                    "snowflake.qa1.snowflakecomputing.com",
                    "443",
                    "sfctest0",
                    "snowman",
                    "PasswordPassword",
                    "snowflake",
                    "TestDB",
                    "TestSchema",
                    "Regress",
                    "AccountAdmin",
                    "1",
                    "qa1",
                    "0"
            },
            // Sending to Dev May receive message Limit exceeded
            // as Amazon SQS has small buffer for dev deployments.
            {
                    "snowflake.local.snowflakecomputing.com",
                    "443",
                    "testaccount",
                    "snowman",
                    "PasswordPassword",
                    "snowflake",
                    "TestDB",
                    "TestSchema",
                    "Regress",
                    "AccountAdmin",
                    "0",
                    "dev",
                    "0"
            },
            // Should fail to send message.
            {
                    "",
                    "443",
                    "sfctest0",
                    "snowman",
                    "PasswordPassword",
                    "snowflake",
                    "TestDB",
                    "TestSchema",
                    "Regress",
                    "AccountAdmin",
                    "1",
                    "Ignore",
                    "-1"
            },
    };

    for (evnt = 0; evnt < sizeof(testcase)/sizeof(SF_SETTINGS); ++evnt) {
        setoobConnectioninfo(
                testcase[evnt].host,
                testcase[evnt].port,
                testcase[evnt].account,
                testcase[evnt].user,
                testcase[evnt].token,
                testcase[evnt].authenticator,
                testcase[evnt].db,
                testcase[evnt].schema,
                testcase[evnt].warehouse,
                testcase[evnt].role,
                strcmp(testcase[evnt].ssl, "1") == 0 ? 1 : 0);

        char connStr[] = "/session/v1/login-request?requestId=c4d53986-ee7a-4f01-9fac-2604653e9c41&request_guid=abbab0e5-5c77-4102-9d29-d44efde6a050&databaseName=testdb&schemaName=testschema&warehouse=regress";
        char url[1024] = {0};
        sprintf(url, "%s:%s%s", testcase[evnt].host, testcase[evnt].port, connStr);
        setConnectionString((char const *) url);

        char msg[512] = {0};
        int statusCode = 1004;
        unsigned long errorCode = 1007;
        snprintf(msg, sizeof(msg), "RestRequest httpPerform HTTP response code not ok status code %d and errorcode %ld",
                 statusCode, errorCode);

        setOOBeventdata(OOBEVENTNAME, (const char *) "HttpResponseError", 0);
        setOOBeventdata(ERRORCODE, NULL, errorCode);
        setOOBeventdata(EXCPMSG, msg, 0);
        setOOBeventdata(REQUESTURL, url, 0);
        setOOBeventdata(OOBSQLSTATE, "HY000", 0);
        setOOBeventdata(OOBCABUNDLE, CABundlePath.c_str(), 0);
        setOOBeventdata(URGENCY, "", 1);
        setOOBeventdata(OOBTRACING, "", 1)
        char *oobevent = prepareOOBevent(nullptr);
        int rc = sendOOBevent(oobevent);
        assert_true(oobevent);
        char *dep = getOOBDeployment();
        assert_string_equal(dep, testcase[evnt].deployment);
        free(oobevent);
        oobevent = nullptr;
        assert_int_equal(rc, atoi(testcase[evnt].rc));
    }
}

int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_oob),
    };
    return cmocka_run_group_tests(tests, nullptr, nullptr);
}
