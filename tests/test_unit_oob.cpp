
// DON'T DELETE THE FOLLOWING LINE
#define CATCH_CONFIG_MAIN

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "utils/test_setup.h"
#include "oobtelemetry.h"
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

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

typedef struct {
    const char *key;
    const char *val;
} SF_PAIR;

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

// Tests DSN logging in OOB
void test_dsn(void **) {
    std::string CABundlePath = getCABundleFile();

    const std::vector<std::string> SF_SENSITIVE_KEYS = std::vector<std::string>{"UID", "PWD", "TOKEN", "PASSCODE", "PRIV_KEY_FILE_PWD"};

    SF_PAIR dsnParameters[] = {
            {"SERVER", "snowflake.local.snowflakecomputing.com"},
            {"PORT", "443"},
            {"ACCOUNT", "testaccount"},
            {"Uid", "snowman"},
            {"PWD", "dummy"},
            {"Token", "TOKEN"},
            {"Authenticator", "snowflake"},
            {"DATABASE", "TestDB"},
            {"SCHEMA", "TestSchema"},
            {"WAREHOUSE", "Regress"},
            {"ROLE", "AccountAdmin"},
            {"SSL", "1"},
    };

    int count = sizeof(dsnParameters)/sizeof(SF_PAIR);
    struct KeyValuePair* kvPairs = (struct KeyValuePair*) malloc(sizeof(struct KeyValuePair)*count);

    for (int i = 0; i < count; ++i) {
        kvPairs[i] = KeyValuePair{dsnParameters[i].key, dsnParameters[i].val};
        for (int j = 0; j < SF_SENSITIVE_KEYS.size(); ++j) {
            if (!strcasecmp(dsnParameters[i].key, SF_SENSITIVE_KEYS[j].c_str())) {
                kvPairs[i] = KeyValuePair{dsnParameters[i].key, "***"};
                break;
            }
        }
    }
    setOOBDsnInfo(kvPairs, count);

    char connStr[] = "/session/v1/login-request?requestId=c4d53986-ee7a-4f01-9fac-2604653e9c41&request_guid=abbab0e5-5c77-4102-9d29-d44efde6a050&databaseName=testdb&schemaName=testschema&warehouse=regress";
    char url[1024] = {0};
    sprintf(url, "%s:%s%s", dsnParameters[0].val, dsnParameters[1].val, connStr);
    setConnectionString((char const *) url);

    setOOBeventdata(OOBEVENTNAME, (const char*)"ConnectionDSNConfig", 0);
    setOOBeventdata(ERRORCODE, NULL, 0);
    setOOBeventdata(EXCPMSG, "", 0);
    setOOBeventdata(REQUESTURL, url, 0);
    setOOBeventdata(OOBSQLSTATE, (const char*)"", 0);
    setOOBeventdata(OOBCABUNDLE, CABundlePath.c_str(), 0);
    char *oobevent = prepareOOBevent(nullptr);

    std::string payload = std::string(oobevent);
    for (int evnt = 0; evnt < sizeof(dsnParameters)/sizeof(SF_PAIR); ++evnt) {
        int idx = payload.find(dsnParameters[evnt].key);
        assert_int_not_equal(idx, std::string::npos);
        idx += strlen(dsnParameters[evnt].key)+strlen("\":");
        while (payload[idx] && payload[idx] != '\"') ++idx;
        idx += strlen("\"");
        int cmp = -1;
        bool isSensitive = false;
        for (int i = 0; i < SF_SENSITIVE_KEYS.size(); ++i) {
            if (!strcasecmp(dsnParameters[evnt].key, SF_SENSITIVE_KEYS[i].c_str())) {
                isSensitive = true;
            }
        }
        if (isSensitive) {
            cmp = payload.compare(idx, 3, "***");
        } else {
            cmp = payload.compare(idx, strlen(dsnParameters[evnt].val), dsnParameters[evnt].val);
        }
        assert_int_equal(cmp, 0);
    }
    free(kvPairs);
    int rc = sendOOBevent(oobevent);
    assert_true(oobevent);
    free(oobevent);
    oobevent = nullptr;
    assert_int_equal(rc, 0);
}

void test_simba(void **) {
    std::string CABundlePath = getCABundleFile();

    SF_PAIR simbaParameters[] = {
            {"DriverManagerEncoding","UTF-16"},
            {"DriverLocale", "en-US"},
            {"ErrorMessagesPath", "/home/debugger/snowflake-odbc/ErrorMessages"},
            {"LogNamespace", ""},
            {"LogPath", "/tmp"},
            {"ODBCInstLib", "libodbcinst.so"},
            {"CURLVerboseMode", "true"},
            {"LogLevel", "6"},
            {"CABundleFile", "/home/debugger/snowflake-odbc/Dependencies/CABundle/cacert.pem"},
    };

    int count = sizeof(simbaParameters)/sizeof(SF_PAIR);
    struct KeyValuePair* kvPairs = (struct KeyValuePair*) malloc(sizeof(struct KeyValuePair)*count);

    for (int i = 0; i < count; ++i) {
        kvPairs[i] = KeyValuePair{simbaParameters[i].key, simbaParameters[i].val};
    }
    setOOBSimbaInfo(kvPairs, count);
    setoobConnectioninfo("snowflake.local.snowflakecomputing.com","","","","","","","","","",0);

    char connStr[] = "/session/v1/login-request?requestId=c4d53986-ee7a-4f01-9fac-2604653e9c41&request_guid=abbab0e5-5c77-4102-9d29-d44efde6a050&databaseName=testdb&schemaName=testschema&warehouse=regress";
    char url[1024] = {0};
    sprintf(url, "snowflake.local.snowflakecomputing.com:443:%s", connStr);
    setConnectionString((char const *) url);

    setOOBeventdata(OOBEVENTNAME, (const char*)"SimbaIniConfig", 0);
    setOOBeventdata(ERRORCODE, NULL, 0);
    setOOBeventdata(EXCPMSG, "", 0);
    setOOBeventdata(REQUESTURL, url, 0);
    setOOBeventdata(OOBSQLSTATE, (const char*)"", 0);
    setOOBeventdata(OOBCABUNDLE, CABundlePath.c_str(), 0);
    char *oobevent = prepareOOBevent(nullptr);

    std::string payload = std::string(oobevent);
    for (int i = 0; i < sizeof(simbaParameters)/sizeof(SF_PAIR); ++i) {
        int idx = payload.find(simbaParameters[i].key);
        assert_int_not_equal(idx, std::string::npos);
        idx += strlen(simbaParameters[i].key)+strlen("\":");
        while (payload[idx] && payload[idx] != '\"') ++idx;
        idx += strlen("\"");
        int cmp = payload.compare(idx, strlen(simbaParameters[i].val), simbaParameters[i].val);
        assert_int_equal(cmp, 0);
    }
    free(kvPairs);
    int rc = sendOOBevent(oobevent);
    assert_true(oobevent);
    free(oobevent);
    oobevent = nullptr;
    assert_int_equal(rc, 0);
}

int main() {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_oob),
            cmocka_unit_test(test_dsn),
            cmocka_unit_test(test_simba),
    };
    return cmocka_run_group_tests(tests, nullptr, nullptr);
}
