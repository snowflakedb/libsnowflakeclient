#include <utility>
#include <thread>
#include <curl/curl.h>
#if defined(_WIN32)
#include <io.h>
#include <direct.h>
#else
#include <dirent.h>
#endif

#include "client_int.h"
#include "snowflake/CurlDescPool.hpp"
#include "utils/test_setup.h"

using namespace ::Snowflake::Client;

static std::string get_cache_dir() {
  char uuid[37] = {0};
  generate_unique_id(uuid);
  std::string tmp_cache_dir;

#if defined(_WIN32)
  const char* win_dir = getenv("WINDIR");
  tmp_cache_dir = std::string(win_dir) + "\\Temp\\crl_cache_" + std::string(uuid);
  _mkdir(tmp_cache_dir.c_str());
#else
  tmp_cache_dir = "/tmp/crl_cache_" + std::string(uuid);
  mkdir(tmp_cache_dir.c_str(), 0700);
#endif

  return tmp_cache_dir;
}

static bool dir_has_files(const std::string& path) {
#if defined(_WIN32)
    std::string search_path = path + "\\*";
    intptr_t handle;
    struct _finddata_t fileinfo;
    handle = _findfirst(search_path.c_str(), &fileinfo);
    if (handle == -1) {
        // Directory does not exist or cannot be opened
        return false;
    }
    do {
        if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
            _findclose(handle);
            return true;
        }
    } while (_findnext(handle, &fileinfo) == 0);
    _findclose(handle);
    return false;
#else
    DIR *dir = opendir(path.c_str());
    if (!dir) return false;
    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            closedir(dir);
            return true;
        }
    }
    closedir(dir);
    return false;
#endif
}

void test_success_with_crl_check(void **unused) {
  SF_UNUSED(unused);

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CRL check
  sf_bool crl_check = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);

  SF_STATUS ret = snowflake_connect(sf);

  // must succeed with success
  assert_int_equal(ret, SF_STATUS_SUCCESS);

  snowflake_term(sf);
}

void test_fail_with_no_crl(void **unused) {
  SF_UNUSED(unused);

  // we need to remove curl cache
  ClientCurlDescPool::getInstance().init();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(6000)));

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  // set env variables for test
  sf_setenv("SF_TEST_CRL_NO_CRL", "true");

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CRL check
  sf_bool crl_check = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);

  // disable advisory, disable allow no crl
  sf_bool crl_enabled = SF_BOOLEAN_FALSE;
  snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_enabled);
  snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_enabled);

  SF_STATUS ret = snowflake_connect(sf);

  // must fail with CURL error
  assert_int_not_equal(ret, SF_STATUS_SUCCESS);
  SF_ERROR_STRUCT *sferr = snowflake_error(sf);
  if (sferr->error_code != SF_STATUS_ERROR_CURL) {
    dump_error(sferr);
  }
  assert_int_equal(sferr->error_code, SF_STATUS_ERROR_CURL);
  assert_string_equal(sferr->msg, "curl_easy_perform() failed: SSL peer certificate or SSH remote key was not OK");

  snowflake_term(sf);
  sf_unsetenv("SF_TEST_CRL_NO_CRL");
}

void test_success_with_no_crl_if_allow_no_crl(void **unused) {
  SF_UNUSED(unused);

  // we need to remove curl cache
  ClientCurlDescPool::getInstance().init();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(6000)));

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  // set env variables for test
  sf_setenv("SF_TEST_CRL_NO_CRL", "true");

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CRL check and allow no crl
  sf_bool crl_check = SF_BOOLEAN_TRUE;
  sf_bool crl_advisory = SF_BOOLEAN_FALSE;
  sf_bool crl_allow_no_crl = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);
  snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_advisory);
  snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_allow_no_crl);

  SF_STATUS ret = snowflake_connect(sf);

  assert_int_equal(ret, SF_STATUS_SUCCESS);

  snowflake_term(sf);
  sf_unsetenv("SF_TEST_CRL_NO_CRL");
}

void test_success_with_no_crl_in_advisory_mode(void **unused) {
  SF_UNUSED(unused);

  // we need to remove curl cache
  ClientCurlDescPool::getInstance().init();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(6000)));

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  // set env variables for test
  sf_setenv("SF_TEST_CRL_NO_CRL", "true");

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CRL check and advisory mode
  sf_bool crl_check = SF_BOOLEAN_TRUE;
  sf_bool crl_advisory = SF_BOOLEAN_TRUE;
  sf_bool crl_allow_no_crl = SF_BOOLEAN_FALSE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);
  snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_advisory);
  snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_allow_no_crl);

  SF_STATUS ret = snowflake_connect(sf);

  assert_int_equal(ret, SF_STATUS_SUCCESS);

  snowflake_term(sf);
  sf_unsetenv("SF_TEST_CRL_NO_CRL");
}

void test_curl_crl_params(void **unused) {
  SF_UNUSED(unused);

  CURL *ch = nullptr;
  assert_int_equal(curl_global_init(CURL_GLOBAL_ALL), CURLE_OK);
  ch = curl_easy_init();
  assert_non_null(ch);

  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DOWNLOAD_TIMEOUT, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_ALLOW_NO_CRL, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_CHECK, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DISK_CACHING, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_MEMORY_CACHING, 1L), CURLE_OK);

  curl_easy_cleanup(ch);
  curl_global_cleanup();
}

void test_crl_cache(void **unused) {
  SF_UNUSED(unused);

  const std::string cache_dir = get_cache_dir();
  sf_setenv("SF_CRL_RESPONSE_CACHE_DIR", cache_dir.c_str());
  assert_true(!dir_has_files(cache_dir));

  CURL *ch = nullptr;
  curl_global_init(CURL_GLOBAL_ALL);
  ch = curl_easy_init();
  assert_non_null(ch);

  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_CHECK, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DISK_CACHING, 1L), CURLE_OK);
  curl_easy_setopt(ch, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
  curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(ch, CURLOPT_URL, "https://snowflake.com");

  int retry = 0;
  CURLcode curlResult;
  while (++retry <= 3 && (curlResult = curl_easy_perform(ch)) != CURLE_OK);
  assert_int_equal(curlResult, CURLE_OK);

  // Check if any CRL has been downloaded
  assert_true(dir_has_files(cache_dir));
  sf_unsetenv("SF_CRL_RESPONSE_CACHE_DIR");
}

void test_no_crl_cache_if_disabled(void **unused) {
  SF_UNUSED(unused);

  const std::string cache_dir = get_cache_dir();
  sf_setenv("SF_CRL_RESPONSE_CACHE_DIR", cache_dir.c_str());
  assert_true(!dir_has_files(cache_dir));

  CURL *ch = nullptr;
  curl_global_init(CURL_GLOBAL_ALL);
  ch = curl_easy_init();
  assert_non_null(ch);

  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_CHECK, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DISK_CACHING, 0L), CURLE_OK);
  curl_easy_setopt(ch, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
  curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(ch, CURLOPT_URL, "https://snowflake.com");

  int retry = 0;
  CURLcode curlResult;
  while (++retry <= 3 && (curlResult = curl_easy_perform(ch)) != CURLE_OK);
  assert_int_equal(curlResult, CURLE_OK);

  // Check if any CRL has been downloaded
  assert_true(!dir_has_files(cache_dir));
  sf_unsetenv("SF_CRL_RESPONSE_CACHE_DIR");
}

int main() {
    initialize_test(SF_BOOLEAN_FALSE);
    constexpr CMUnitTest tests[] = {
      cmocka_unit_test(test_success_with_crl_check),
      cmocka_unit_test(test_fail_with_no_crl),
      cmocka_unit_test(test_success_with_no_crl_if_allow_no_crl),
      cmocka_unit_test(test_success_with_no_crl_in_advisory_mode),
      cmocka_unit_test(test_curl_crl_params),
      cmocka_unit_test(test_crl_cache),
      cmocka_unit_test(test_no_crl_cache_if_disabled)
    };
    int ret = cmocka_run_group_tests(tests, nullptr, nullptr);
    snowflake_global_term();
    return ret;
}
