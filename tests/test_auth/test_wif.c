#include <stdlib.h>
#include <string.h>
#include "auth_utils.h"

// Checks if running in a specific cloud environment
static int is_cloud_env(const char *expected_type) {
    char *type = getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE");
    if (!type) {
        return 0;
    }
    return strcasecmp(type, expected_type) == 0;
}

// Executes a simple query to verify connection works
static void verify_connection_works(SF_CONNECT *sf) {
    SF_STMT *stmt = snowflake_stmt(sf);
    assert_non_null(stmt);
    
    SF_STATUS status = snowflake_query(stmt, "SELECT 1 as test_col", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(stmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_fetch(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    const char *value = NULL;
    snowflake_column_as_const_str(stmt, 1, &value);
    assert_string_equal(value, "1");
    
    snowflake_stmt_term(stmt);
}

static char* get_current_user(SF_CONNECT *sf) {
    SF_STMT *stmt = snowflake_stmt(sf);
    assert_non_null(stmt);
    
    SF_STATUS status = snowflake_query(stmt, "SELECT CURRENT_USER()", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    
    status = snowflake_fetch(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    
    const char *user = NULL;
    snowflake_column_as_const_str(stmt, 1, &user);
    
    char *result = strdup(user);
    snowflake_stmt_term(stmt);
    return result;
}

// The cloud tests mirror tests from test_manual_wif.cpp
void test_aws_wif_authentication(void **unused) {
    SF_UNUSED(unused);
    
    if (!is_cloud_env("AWS")) {
        fprintf(stderr, "Not running AWS WIF test. SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE is not AWS\n");
        skip();
        return;
    }
    
    fprintf(stderr, "Testing AWS WIF authentication via C API\n");
    
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);

    const SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    
    fprintf(stderr, "AWS WIF connection successful\n");
    
    verify_connection_works(sf);
    
    char *user = get_current_user(sf);
    fprintf(stderr, "Authenticated as: %s\n", user);
    assert_non_null(user);
    assert_true(strlen(user) > 0);
    free(user);
    
    snowflake_term(sf);
}

void test_gcp_wif_authentication(void **unused) {
    SF_UNUSED(unused);
    
    if (!is_cloud_env("GCP")) {
        fprintf(stderr, "Not running GCP WIF test. SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE is not GCP\n");
        skip();
        return;
    }
    
    fprintf(stderr, "Testing GCP WIF authentication via C API\n");
    
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);

    const SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    
    fprintf(stderr, "GCP WIF connection successful\n");

    verify_connection_works(sf);

    char *user = get_current_user(sf);
    fprintf(stderr, "Authenticated as: %s\n", user);
    assert_non_null(user);
    assert_true(strlen(user) > 0);
    free(user);
    
    snowflake_term(sf);
}

void test_azure_wif_authentication(void **unused) {
    SF_UNUSED(unused);
    
    if (!is_cloud_env("AZURE")) {
        fprintf(stderr, "Not running Azure WIF test. SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE is not AZURE\n");
        skip();
        return;
    }
    
    char *resource = getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_RESOURCE");
    if (!resource) {
        fprintf(stderr, "SNOWFLAKE_WIF_ATTESTATION_TEST_RESOURCE is not set\n");
        skip();
        return;
    }
    
    fprintf(stderr, "Testing Azure WIF authentication via C API\n");
    fprintf(stderr, "Using resource: %s\n", resource);
    
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);

    const SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    
    fprintf(stderr, "Azure WIF connection successful\n");

    verify_connection_works(sf);

    char *user = get_current_user(sf);
    fprintf(stderr, "Authenticated as: %s\n", user);
    assert_non_null(user);
    assert_true(strlen(user) > 0);
    free(user);
    
    snowflake_term(sf);
}

// Test that WIF authentication fails gracefully when not in cloud environment
void test_wif_no_cloud_credentials(void **unused) {
    SF_UNUSED(unused);

    const char *attestation_type = getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE");
    if (attestation_type) {
        fprintf(stderr, "Skipping no-credentials test - running in cloud environment\n");
        skip();
        return;
    }

    fprintf(stderr, "Testing WIF authentication failure without cloud credentials\n");

    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);

    const SF_STATUS status = snowflake_connect(sf);

    assert_int_not_equal(status, SF_STATUS_SUCCESS);

    const SF_ERROR_STRUCT *error = snowflake_error(sf);
    fprintf(stderr, "Expected error: %s\n", error->msg);

    snowflake_term(sf);
}

// Test that invalid authenticator string is rejected
void test_wif_invalid_authenticator(void **unused) {
    SF_UNUSED(unused);
    
    fprintf(stderr, "Testing invalid authenticator string\n");
    
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);
    
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "invalid_workload_identity");
    
    const SF_STATUS status = snowflake_connect(sf);
    
    // Should fail with unsupported authenticator or connection error
    assert_int_not_equal(status, SF_STATUS_SUCCESS);
    
    SF_ERROR_STRUCT *error = snowflake_error(sf);
    fprintf(stderr, "Expected error: %s\n", error->msg);
    
    snowflake_term(sf);
}

// Test that authenticator type is correctly recognized
void test_wif_valid_authenticator(void **unused) {
    SF_UNUSED(unused);
    
    fprintf(stderr, "Testing WIF authenticator type recognition\n");
    
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);
    
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);
    
    // May fail if not in cloud, but shouldn't fail with "unsupported authenticator"
    const SF_STATUS status = snowflake_connect(sf);
    const SF_ERROR_STRUCT *error = snowflake_error(sf);

    if (status != SF_STATUS_SUCCESS) {
        fprintf(stderr, "Connection failed with: %s\n", error->msg);
        assert_true(strstr(error->msg, "unsupported authenticator") == NULL);
    }
    
    snowflake_term(sf);
}

// Test multiple connections using WIF
void test_wif_multiple_connections(void **unused) {
    SF_UNUSED(unused);
    
    // Only run in cloud environment
    char *attestation_type = getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE");
    if (!attestation_type) {
        fprintf(stderr, "Skipping multiple connections test - not in cloud environment\n");
        skip();
        return;
    }
    
    fprintf(stderr, "Testing multiple WIF connections\n");
    
    // First connection
    SF_CONNECT *sf1 = snowflake_init();
    set_all_snowflake_attributes(sf1);
    snowflake_set_attribute(sf1, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);
    
    const SF_STATUS status1 = snowflake_connect(sf1);
    if (status1 != SF_STATUS_SUCCESS) {
        dump_error(&(sf1->error));
    }
    assert_int_equal(status1, SF_STATUS_SUCCESS);
    
    // Second connection
    SF_CONNECT *sf2 = snowflake_init();
    set_all_snowflake_attributes(sf2);
    snowflake_set_attribute(sf2, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);
    
    const SF_STATUS status2 = snowflake_connect(sf2);
    if (status2 != SF_STATUS_SUCCESS) {
        dump_error(&(sf2->error));
    }
    assert_int_equal(status2, SF_STATUS_SUCCESS);

    verify_connection_works(sf1);
    verify_connection_works(sf2);
    
    snowflake_term(sf1);
    snowflake_term(sf2);
    
    fprintf(stderr, "Multiple WIF connections successful\n");
}

// Test explicit provider specification
void test_wif_explicit_provider_integration(void **unused) {
    SF_UNUSED(unused);
    
    char *attestation_type = getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE");
    if (!attestation_type) {
        fprintf(stderr, "Skipping explicit provider test - not in cloud environment\n");
        skip();
        return;
    }
    
    fprintf(stderr, "Testing explicit WIF provider specification\n");
    
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);
    
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);
    // Explicit provider matching the environment
    snowflake_set_attribute(sf, SF_CON_WIF_PROVIDER, attestation_type);
    
    // Azure resource if available
    if (strcmp(attestation_type, "AZURE") == 0) {
        char *resource = getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_RESOURCE");
        if (resource) {
            snowflake_set_attribute(sf, SF_CON_WIF_AZURE_RESOURCE, resource);
        }
    }
    
    const SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    
    fprintf(stderr, "Explicit provider '%s' worked correctly\n", attestation_type);
    verify_connection_works(sf);
    snowflake_term(sf);
}

// Test invalid provider specification falls back to auto-detection
void test_wif_invalid_provider_fallback(void **unused) {
    SF_UNUSED(unused);
    
    char *attestation_type = getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE");
    if (!attestation_type) {
        fprintf(stderr, "Skipping provider fallback test - not in cloud environment\n");
        skip();
        return;
    }
    
    fprintf(stderr, "Testing invalid provider fallback to auto-detection\n");
    
    SF_CONNECT *sf = snowflake_init();
    set_all_snowflake_attributes(sf);
    
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_WORKLOAD_IDENTITY);
    snowflake_set_attribute(sf, SF_CON_WIF_PROVIDER, "INVALID_PROVIDER");
    
    // Should succeed by falling back to auto-detection
    const SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    
    verify_connection_works(sf);
    snowflake_term(sf);
}

// Test get_attribute for WIF fields
void test_wif_get_attributes(void **unused) {
    SF_UNUSED(unused);
    
    fprintf(stderr, "Testing WIF get_attribute functions\n");
    
    SF_CONNECT *sf = snowflake_init();
    
    const char *provider = "AWS";
    const char *token = "test_token";
    const char *resource = "test_resource";
    
    snowflake_set_attribute(sf, SF_CON_WIF_PROVIDER, provider);
    snowflake_set_attribute(sf, SF_CON_WIF_TOKEN, token);
    snowflake_set_attribute(sf, SF_CON_WIF_AZURE_RESOURCE, resource);
    
    char *retrieved_provider = NULL;
    char *retrieved_token = NULL;
    char *retrieved_resource = NULL;
    
    snowflake_get_attribute(sf, SF_CON_WIF_PROVIDER, (void**)&retrieved_provider);
    snowflake_get_attribute(sf, SF_CON_WIF_TOKEN, (void**)&retrieved_token);
    snowflake_get_attribute(sf, SF_CON_WIF_AZURE_RESOURCE, (void**)&retrieved_resource);
    
    assert_non_null(retrieved_provider);
    assert_string_equal(retrieved_provider, provider);
    
    assert_non_null(retrieved_token);
    assert_string_equal(retrieved_token, token);
    
    assert_non_null(retrieved_resource);
    assert_string_equal(retrieved_resource, resource);
    
    snowflake_term(sf);
    
    fprintf(stderr, "WIF get_attribute tests passed\n");
}

