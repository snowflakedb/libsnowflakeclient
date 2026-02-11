
#include <string.h>
#include "../utils/test_setup.h"
#include "../utils/mock_setup.h"
#include <unistd.h>

const size_t MAX_PATH_LEN = 1024;

char* load_data(const char* filename) {
  char path[MAX_PATH_LEN];
  snprintf(path, MAX_PATH_LEN, "./mock/test_data/%s", filename);
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    log_error("Failed to open the file %s", path);
    return NULL;
  }
  fseek(fp, 0, SEEK_END);
  size_t fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *string = malloc(fsize + 1);
  fread(string, fsize, 1, fp);
  fclose(fp);
  return string;
}

char* first_mfa_request = NULL;
char* first_mfa_response = NULL;
char* second_mfa_request = NULL;

void setup_mfa_connect_initial_request_mock() {
  expect_string(__wrap_http_perform, url, "https://host:443/session/v1/login-request");
  expect_string(__wrap_http_perform, body, first_mfa_request);
  expect_string(__wrap_http_perform, request_type_str, "POST");
  expect_value(__wrap_http_perform, header->header_service_name, NULL);
  expect_value(__wrap_http_perform, header->header_token, NULL);
  will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(first_mfa_response));
}

void setup_mfa_term_initial_request_mock() {
  expect_string(__wrap_http_perform, url, "https://host:443/session");
  expect_value(__wrap_http_perform, body, NULL);
  expect_string(__wrap_http_perform, request_type_str, "POST");
  expect_value(__wrap_http_perform, header->header_service_name, NULL);
  expect_string(__wrap_http_perform, header->header_token, "Authorization: Snowflake Token=\"TOKEN\"");
  will_return(__wrap_http_perform, "{}");
}

void setup_mfa_connect_cached_mfa_request_mock() {
  expect_string(__wrap_http_perform, url, "https://host:443/session/v1/login-request");
  expect_string(__wrap_http_perform, body, second_mfa_request);
  expect_string(__wrap_http_perform, request_type_str, "POST");
  expect_value(__wrap_http_perform, header->header_service_name, NULL);
  expect_value(__wrap_http_perform, header->header_token, NULL);
  will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(first_mfa_response));
}

void setup_mfa_term_cached_mfa_request_mock() {
  expect_string(__wrap_http_perform, url, "https://host:443/session");
  expect_value(__wrap_http_perform, body, NULL);
  expect_string(__wrap_http_perform, request_type_str, "POST");
  expect_value(__wrap_http_perform, header->header_service_name, NULL);
  expect_string(__wrap_http_perform, header->header_token, "Authorization: Snowflake Token=\"TOKEN\"");
  will_return(__wrap_http_perform, "{}");
}

#define ACCOUNT "account"
#define HOST "host"
#define USER "user"

SF_CONNECT* sf_connect_init() {
  SF_CONNECT* sf = snowflake_init();
  snowflake_set_attribute(sf, SF_CON_ACCOUNT,ACCOUNT);
  snowflake_set_attribute(sf, SF_CON_HOST, HOST);
  snowflake_set_attribute(sf, SF_CON_USER, USER);
  snowflake_set_attribute(sf, SF_CON_PORT, "443");
  snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
  return sf;
}

void test_mfa_token_caching(void **unused) {
  sf_setenv("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", "sf_temporary_credential_cache_dir");
  rmdir("sf_temporary_credential_cache_dir");
  mkdir("sf_temporary_credential_cache_dir", 0700);
  secure_storage_ptr ss = secure_storage_init();
  secure_storage_remove_credential(ss, HOST, USER, MFA_TOKEN);

  {
    SF_CONNECT *sf = sf_connect_init();
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "passwd");
    snowflake_set_attribute(sf, SF_CON_PASSCODE, "passcode");
    sf_bool client_request_mfa_token = 1;
    snowflake_set_attribute(sf, SF_CON_CLIENT_REQUEST_MFA_TOKEN, &client_request_mfa_token);
    sf_bool client_keep_alive = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_CLIENT_SESSION_KEEP_ALIVE, &client_keep_alive);
    setup_mfa_connect_initial_request_mock();
    SF_STATUS status = snowflake_connect(sf);

    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sf->error));
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);
    setup_mfa_term_initial_request_mock();
    snowflake_term(sf);
  }

  {
    SF_CONNECT *sf = sf_connect_init();
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "passwd");
    snowflake_set_attribute(sf, SF_CON_PASSCODE, "passcode");
    sf_bool client_request_mfa_token = 1;
    snowflake_set_attribute(sf, SF_CON_CLIENT_REQUEST_MFA_TOKEN, &client_request_mfa_token);
    sf_bool client_keep_alive = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_CLIENT_SESSION_KEEP_ALIVE, &client_keep_alive);
    setup_mfa_connect_cached_mfa_request_mock();
    SF_STATUS status = snowflake_connect(sf);

    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sf->error));
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);
    setup_mfa_term_cached_mfa_request_mock();
    snowflake_term(sf);
  }
}

  int main(void) {
    first_mfa_request = load_data("expected_first_mfa_request.json");
    first_mfa_response = load_data("first_mfa_response.json");
    second_mfa_request = load_data("expected_second_mfa_request.json");
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mfa_token_caching),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    free(first_mfa_request);
    free(first_mfa_response);
    free(second_mfa_request);
    return ret;
  }
