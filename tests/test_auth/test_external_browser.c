#include "auth_utils.h"


typedef struct {
    int status;
    SF_ERROR_STRUCT error;
} threadConnectResult;

typedef struct {
    SF_CONNECT *sf;
    int timeout;
} threadConnectArg;

void runCommand(const char *scriptPath, const char *args) {
    char command[1024];
    const char *arguments = (args != NULL) ? args : "";

    int n = snprintf(command, sizeof(command), "node %s %s", scriptPath, arguments);
    if (n < 0 || n >= (int)sizeof(command)) {
        fprintf(stderr, "Failed to build command string\n");
        return;
    }
    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "Failed to execute script: %s\n", scriptPath);
    }
}

void *provideCredentials(void *arg) {
    char **creds = (char **)arg;
    const char *scenario = creds[0];
    const char *login = creds[1];
    const char *password = creds[2];

    char fullArgs[512];
    snprintf(fullArgs, sizeof(fullArgs), "%s %s %s", scenario, login, password);
    runCommand("/externalbrowser/provideBrowserCredentials.js", fullArgs);
    return NULL;
}

void *threadConnect(void *arg) {
    threadConnectArg *connectArg = (threadConnectArg *)arg;
    SF_CONNECT *sf = connectArg->sf;

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    if (connectArg->timeout > 0) {
        snowflake_set_attribute(sf, SF_CON_BROWSER_RESPONSE_TIMEOUT, &connectArg->timeout);
    }

    SF_STATUS status = snowflake_connect(sf);
    threadConnectResult *result = malloc(sizeof(threadConnectResult));
    result->error = sf->error;
    result->status = (int)status;
    return result;
}

void cleanBrowserProcesses(void) {
    const char *cleanBrowserProcessesPath = "/externalbrowser/cleanBrowserProcesses.js";
    runCommand(cleanBrowserProcessesPath, "");
}

void test_external_browser_successful_connection(void **unused) {
    SF_UNUSED(unused);
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);
    pthread_t connect_thread, provide_credentials_thread;
    threadConnectArg connectArg;
    connectArg.sf = sf;
    void *connect_result;
    char *args[] = {
            "success",
            getenv("SNOWFLAKE_AUTH_TEST_BROWSER_USER"),
            getenv("SNOWFLAKE_AUTH_TEST_OKTA_PASS")
    };

    pthread_create(&connect_thread, NULL, threadConnect, &connectArg);
    pthread_create(&provide_credentials_thread, NULL, provideCredentials, args);
    pthread_join(provide_credentials_thread, NULL);
    pthread_join(connect_thread, &connect_result);
    
    int status = *((int *)connect_result);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    free(connect_result);
    pthread_cancel(provide_credentials_thread);
    pthread_cancel(connect_thread);
    snowflake_term(sf);
    cleanBrowserProcesses();
}

void test_external_browser_mismatched_username(void **unused) {
    SF_UNUSED(unused);
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);
    snowflake_set_attribute(sf, SF_CON_USER, "differentUsername");
    pthread_t connect_thread, provide_credentials_thread;
    threadConnectArg connectArg;
    connectArg.sf = sf;
    void *connect_result;
    char *args[] = {
            "success",
            getenv("SNOWFLAKE_AUTH_TEST_BROWSER_USER"),
            getenv("SNOWFLAKE_AUTH_TEST_OKTA_PASS")
    };

    pthread_create(&connect_thread, NULL, threadConnect, &connectArg);
    pthread_create(&provide_credentials_thread, NULL, provideCredentials, args);
    pthread_join(provide_credentials_thread, NULL);
    pthread_join(connect_thread, &connect_result);

    threadConnectResult *result = (threadConnectResult *)connect_result;
    assert_int_equal(result->status, SF_STATUS_ERROR_GENERAL);

    SF_ERROR_STRUCT *error = &(result->error);
    assert_true(strstr(error->msg, "The user you were trying to authenticate as differs from the user currently logged in at the IDP.") != NULL);

    snowflake_term(sf);
    free(connect_result);
    pthread_cancel(provide_credentials_thread);
    pthread_cancel(connect_thread);
    cleanBrowserProcesses();
}

//todo SNOW-2004277 return more descriptive error message
void test_external_browser_wrong_credentials(void **unused) {
    SF_UNUSED(unused);
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);
    threadConnectArg connectArg;
    connectArg.sf = sf;
    connectArg.timeout = 5;

    pthread_t connect_thread, provide_credentials_thread;
    void *connect_result;
    char *args[] = {
            "fail",
            "itsnotanaccount.com",
            "fakepassword"
    };

    pthread_create(&connect_thread, NULL, threadConnect, &connectArg);
    pthread_create(&provide_credentials_thread, NULL, provideCredentials, args);
    pthread_join(provide_credentials_thread, NULL);
    pthread_join(connect_thread, &connect_result);

    threadConnectResult *result = (threadConnectResult *)connect_result;
    assert_int_equal(result->status, SF_STATUS_ERROR_GENERAL);

    const SF_ERROR_STRUCT *error = &(result->error);
    // Accept either browser timeout or Snowflake SAML error
    const sf_bool is_timeout_error = (sf_bool)(strstr(error->msg, "SFAuthWebBrowserFailed") != NULL);
    const sf_bool is_saml_error = (sf_bool)(strstr(error->msg, "SAML response is invalid") != NULL);
    assert_true(is_timeout_error || is_saml_error);

    snowflake_term(sf);
    free(result);
    pthread_cancel(provide_credentials_thread);
    pthread_cancel(connect_thread);
    cleanBrowserProcesses();
}
