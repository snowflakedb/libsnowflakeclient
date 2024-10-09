
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

void setup_mfa_connect_mock_1() {
  expect_string(__wrap_http_perform, url, "https://host:443/session/v1/login-request");
  expect_string(__wrap_http_perform, body, first_mfa_request);
  expect_string(__wrap_http_perform, request_type_str, "POST");
  expect_value(__wrap_http_perform, header->header_service_name, NULL);
  expect_value(__wrap_http_perform, header->header_token, NULL);
  will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(first_mfa_response));
}

void setup_mfa_term_mock_1() {
  expect_string(__wrap_http_perform, url, "https://host:443/session");
  expect_value(__wrap_http_perform, body, NULL);
  expect_string(__wrap_http_perform, request_type_str, "POST");
  expect_value(__wrap_http_perform, header->header_service_name, NULL);
  expect_string(__wrap_http_perform, header->header_token, "Authorization: Snowflake Token=\"TOKEN\"");
  will_return(__wrap_http_perform, "{}");
}

void setup_mfa_connect_mock_2() {
  expect_string(__wrap_http_perform, url, "https://host:443/session/v1/login-request");
  expect_string(__wrap_http_perform, body, second_mfa_request);
  expect_string(__wrap_http_perform, request_type_str, "POST");
  expect_value(__wrap_http_perform, header->header_service_name, NULL);
  expect_value(__wrap_http_perform, header->header_token, NULL);
  will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(first_mfa_response));
}

void setup_mfa_term_mock_2() {
  expect_string(__wrap_http_perform, url, "https://host:443/session");
  expect_value(__wrap_http_perform, body, NULL);
  expect_string(__wrap_http_perform, request_type_str, "POST");
  expect_value(__wrap_http_perform, header->header_service_name, NULL);
  expect_string(__wrap_http_perform, header->header_token, "Authorization: Snowflake Token=\"TOKEN\"");
  will_return(__wrap_http_perform, "{}");
}

SF_CONNECT* sf_connect_init() {
  SF_CONNECT* sf = snowflake_init();
  snowflake_set_attribute(sf, SF_CON_ACCOUNT,"account");
  snowflake_set_attribute(sf, SF_CON_USER, "user");
  snowflake_set_attribute(sf, SF_CON_HOST, "host");
  snowflake_set_attribute(sf, SF_CON_PORT, "443");
  snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
  return sf;
}

void test_mfa_token_caching(void **unused) {
  {
    SF_CONNECT *sf = sf_connect_init();
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "passwd");
    snowflake_set_attribute(sf, SF_CON_PASSCODE, "passcode");
    setup_mfa_connect_mock_1();
    SF_STATUS status = snowflake_connect(sf);

    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sf->error));
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);
    setup_mfa_term_mock_1();
    snowflake_term(sf);
  }

  {
    SF_CONNECT *sf = sf_connect_init();
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "passwd");
    snowflake_set_attribute(sf, SF_CON_PASSCODE, "passcode");
    setup_mfa_connect_mock_2();
    SF_STATUS status = snowflake_connect(sf);

    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sf->error));
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);
    setup_mfa_term_mock_2();
    snowflake_term(sf);
  }
}

void compare(const char* a, const char* b) {
  int pos = 1;
  int line = 1;
  for (int i = 0; 1; i++) {
    if (a[i] != b[i]) {
      log_error("Line: %d, pos: %d :: %d != %d", line, pos, a[i], b[i]);
      break;
    }
    pos++;
    if (a[i] == '\0') {
      break;
    }

    if (a[i] == '\n') {
      pos = 1;
      line++;
    }
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
    return ret;
  }
