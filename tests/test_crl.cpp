#include <utility>
#include <thread>
#include <fstream>
#include <ctime>
#include <curl/curl.h>
#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#if defined(_WIN32)
#include <io.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "client_int.h"
#include "snowflake/CurlDescPool.hpp"
#include "utils/test_setup.h"
#include "EnvOverride.hpp"

extern "C" {
#ifdef _WIN32
void __stdcall cleanupCertCRLCache(void);
#else
void cleanupCertCRLCache(void);
#endif
}

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

  SF_CONNECT *sf;
  SF_STATUS ret;
  {
    // set env variables for test
    EnvOverride override("SF_TEST_CRL_NO_CRL", "true");

    sf = setup_snowflake_connection();

    // enable CRL check
    sf_bool crl_check = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);

    // disable advisory, disable allow no crl
    sf_bool crl_enabled = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_enabled);
    snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_enabled);

    ret = snowflake_connect(sf);
  }


  // must fail with CURL error
  assert_int_not_equal(ret, SF_STATUS_SUCCESS);
  SF_ERROR_STRUCT *sferr = snowflake_error(sf);
  if (sferr->error_code != SF_STATUS_ERROR_CURL) {
    dump_error(sferr);
  }
  assert_int_equal(sferr->error_code, SF_STATUS_ERROR_CURL);
  assert_string_equal(sferr->msg, "curl_easy_perform() failed: SSL peer certificate or SSH remote key was not OK");

  snowflake_term(sf);
}

void test_success_with_no_crl_if_allow_no_crl(void **unused) {
  SF_UNUSED(unused);

  // we need to remove curl cache
  ClientCurlDescPool::getInstance().init();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(6000)));

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  SF_STATUS ret;
  {
    // set env variables for test
    EnvOverride override("SF_TEST_CRL_NO_CRL", "true");

    SF_CONNECT *sf = setup_snowflake_connection();

    // enable CRL check and allow no crl
    sf_bool crl_check = SF_BOOLEAN_TRUE;
    sf_bool crl_advisory = SF_BOOLEAN_FALSE;
    sf_bool crl_allow_no_crl = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);
    snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_advisory);
    snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_allow_no_crl);

    ret = snowflake_connect(sf);
    snowflake_term(sf);
  }

  assert_int_equal(ret, SF_STATUS_SUCCESS);
}

void test_success_with_no_crl_in_advisory_mode(void **unused) {
  SF_UNUSED(unused);

  // we need to remove curl cache
  ClientCurlDescPool::getInstance().init();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(6000)));

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  SF_STATUS ret;
  {
    // set env variables for test
    EnvOverride override("SF_TEST_CRL_NO_CRL", "true");

    SF_CONNECT *sf = setup_snowflake_connection();

    // enable CRL check and advisory mode
    sf_bool crl_check = SF_BOOLEAN_TRUE;
    sf_bool crl_advisory = SF_BOOLEAN_TRUE;
    sf_bool crl_allow_no_crl = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);
    snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_advisory);
    snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_allow_no_crl);

    ret = snowflake_connect(sf);
    snowflake_term(sf);
  }

  assert_int_equal(ret, SF_STATUS_SUCCESS);
}

void test_curl_crl_params(void **unused) {
  SF_UNUSED(unused);

  CURL *ch = nullptr;
  ch = curl_easy_init();
  assert_non_null(ch);

  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DOWNLOAD_TIMEOUT, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_ALLOW_NO_CRL, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_CHECK, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DISK_CACHING, 1L), CURLE_OK);
  assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_MEMORY_CACHING, 1L), CURLE_OK);

  CURLcode maxSizeRes = curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DOWNLOAD_MAX_SIZE, (long)(100 * 1024 * 1024));
  assert_true(maxSizeRes == CURLE_OK || maxSizeRes == CURLE_UNKNOWN_OPTION);

  curl_easy_cleanup(ch);
}

void test_crl_download_max_size_attribute(void **unused) {
  SF_UNUSED(unused);

  SF_CONNECT *sf = setup_snowflake_connection();

  // default should be SF_CRL_DOWNLOAD_MAX_SIZE_DEFAULT (20 MB)
  void *val = NULL;
  snowflake_get_attribute(sf, SF_CON_CRL_DOWNLOAD_MAX_SIZE, &val);
  assert_non_null(val);
  assert_int_equal(*(long*)val, SF_CRL_DOWNLOAD_MAX_SIZE_DEFAULT);

  // set a custom value
  int64 custom_size = 50 * 1024 * 1024; // 50 MB
  snowflake_set_attribute(sf, SF_CON_CRL_DOWNLOAD_MAX_SIZE, &custom_size);

  val = NULL;
  snowflake_get_attribute(sf, SF_CON_CRL_DOWNLOAD_MAX_SIZE, &val);
  assert_non_null(val);
  assert_int_equal(*(long*)val, 50 * 1024 * 1024);

  // set with NULL value should reset to default
  snowflake_set_attribute(sf, SF_CON_CRL_DOWNLOAD_MAX_SIZE, NULL);

  val = NULL;
  snowflake_get_attribute(sf, SF_CON_CRL_DOWNLOAD_MAX_SIZE, &val);
  assert_non_null(val);
  assert_int_equal(*(long*)val, SF_CRL_DOWNLOAD_MAX_SIZE_DEFAULT);

  snowflake_term(sf);
}

void test_success_with_crl_check_custom_max_size(void **unused) {
  SF_UNUSED(unused);

  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  SF_CONNECT *sf = setup_snowflake_connection();

  sf_bool crl_check = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);

  int64 custom_size = 100 * 1024 * 1024; // 100 MB
  snowflake_set_attribute(sf, SF_CON_CRL_DOWNLOAD_MAX_SIZE, &custom_size);

  SF_STATUS ret = snowflake_connect(sf);
  assert_int_equal(ret, SF_STATUS_SUCCESS);

  snowflake_term(sf);
}

void test_crl_cache(void **unused) {
  SF_UNUSED(unused);

/*
 * CURLSSLOPT_NATIVE_CA should work with openssl on Windows only
 * https://curl.se/libcurl/c/CURLOPT_SSL_OPTIONS.html
 * but somehow it works on rhel so disable the test case on Ubuntu
 * when the required certificate is missing (in a different location
 * on Ubuntu).
 */
#ifdef __linux__
  if (access("/etc/pki/tls/certs/ca-bundle.crt", F_OK) != 0)
  {
    return;
  }
#endif

  const std::string cache_dir = get_cache_dir();

  CURL *ch = nullptr;
  {
    EnvOverride override("SF_CRL_RESPONSE_CACHE_DIR", cache_dir);
    assert_true(!dir_has_files(cache_dir));

    ch = curl_easy_init();
    assert_non_null(ch);

    assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_CHECK, 1L), CURLE_OK);
    assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DISK_CACHING, 1L), CURLE_OK);
    curl_easy_setopt(ch, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(ch, CURLOPT_URL, "https://snowflake.com");

    CURLcode curlResult = curl_easy_perform(ch);
    assert_int_equal(curlResult, CURLE_OK);
  }

  curl_easy_cleanup(ch);
  // Check if any CRL has been downloaded
  assert_true(dir_has_files(cache_dir));
}

static bool file_exists(const std::string& path) {
  return access(path.c_str(), F_OK) == 0;
}

static std::string join_path(const std::string& dir, const std::string& name) {
#if defined(_WIN32)
  return dir + "\\" + name;
#else
  return dir + "/" + name;
#endif
}

static EVP_PKEY *generate_test_key() {
  EVP_PKEY *key = nullptr;
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  if (!ctx) {
    return nullptr;
  }

  if (EVP_PKEY_keygen_init(ctx) <= 0 ||
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) <= 0 ||
      EVP_PKEY_keygen(ctx, &key) <= 0) {
    EVP_PKEY_free(key);
    key = nullptr;
  }
  EVP_PKEY_CTX_free(ctx);
  return key;
}

// With bogus_next_update the nextUpdate field is a well-formed UTCTIME holding
// an out-of-range date, so ASN1_TIME_cmp_time_t fails on it instead of
// reporting an ordering.
static bool write_test_crl(const std::string& path, time_t last_update,
                           time_t next_update, bool bogus_next_update = false) {
  X509_CRL *crl = X509_CRL_new();
  X509_NAME *issuer = X509_NAME_new();
  ASN1_TIME *last = ASN1_TIME_set(NULL, last_update);
  ASN1_TIME *next = nullptr;
  EVP_PKEY *key = generate_test_key();
  BIO *bio = nullptr;
  bool ok = false;

  if (bogus_next_update) {
    next = ASN1_STRING_type_new(V_ASN1_UTCTIME);
    if (next && !ASN1_STRING_set(next, "999999999999Z", 13)) {
      ASN1_STRING_free(next);
      next = nullptr;
    }
  } else {
    next = ASN1_TIME_set(NULL, next_update);
  }

  if (crl && issuer && last && next && key &&
      X509_NAME_add_entry_by_txt(issuer, "CN", MBSTRING_ASC,
                                 (const unsigned char *)"test", -1, -1, 0) &&
      X509_CRL_set_issuer_name(crl, issuer) &&
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      X509_CRL_set1_lastUpdate(crl, last) &&
      X509_CRL_set1_nextUpdate(crl, next) &&
#else
      X509_CRL_set_lastUpdate(crl, last) &&
      X509_CRL_set_nextUpdate(crl, next) &&
#endif
      X509_CRL_sign(crl, key, EVP_sha256()) > 0) {
    bio = BIO_new_file(path.c_str(), "w");
    ok = bio && PEM_write_bio_X509_CRL(bio, crl) == 1;
  }

  BIO_free(bio);
  EVP_PKEY_free(key);
  ASN1_STRING_free(next);
  ASN1_STRING_free(last);
  X509_NAME_free(issuer);
  X509_CRL_free(crl);
  return ok;
}

void test_crl_cache_cleanup_removes_expired(void **unused) {
  SF_UNUSED(unused);

  const std::string cache_dir = get_cache_dir();
  const std::string expired_path = join_path(cache_dir, "expired.crl");
  const time_t now = time(NULL);
  assert_true(write_test_crl(expired_path, now - 7200, now - 3600));
  assert_true(file_exists(expired_path));

  {
    EnvOverride cache_dir_env("SF_CRL_RESPONSE_CACHE_DIR", cache_dir);
    EnvOverride delay_env("SF_CRL_ON_DISK_CACHE_REMOVAL_DELAY", "0");
    cleanupCertCRLCache();
  }

  assert_true(!file_exists(expired_path));
}

void test_crl_cache_cleanup_keeps_fresh(void **unused) {
  SF_UNUSED(unused);

  const std::string cache_dir = get_cache_dir();
  const std::string fresh_path = join_path(cache_dir, "fresh.crl");
  const time_t now = time(NULL);
  assert_true(write_test_crl(fresh_path, now - 3600, now + 86400));
  assert_true(file_exists(fresh_path));

  {
    EnvOverride cache_dir_env("SF_CRL_RESPONSE_CACHE_DIR", cache_dir);
    EnvOverride delay_env("SF_CRL_ON_DISK_CACHE_REMOVAL_DELAY", "0");
    cleanupCertCRLCache();
  }

  assert_true(file_exists(fresh_path));
}

void test_crl_cache_cleanup_removes_corrupt(void **unused) {
  SF_UNUSED(unused);

  const std::string cache_dir = get_cache_dir();
  const std::string corrupt_path = join_path(cache_dir, "corrupt.crl");
  {
    std::ofstream out(corrupt_path.c_str());
    out << "not a crl";
  }
  assert_true(file_exists(corrupt_path));

  {
    EnvOverride cache_dir_env("SF_CRL_RESPONSE_CACHE_DIR", cache_dir);
    EnvOverride delay_env("SF_CRL_ON_DISK_CACHE_REMOVAL_DELAY", "0");
    cleanupCertCRLCache();
  }

  assert_true(!file_exists(corrupt_path));
}

void test_crl_cache_cleanup_keeps_unparseable_next_update(void **unused) {
  SF_UNUSED(unused);

  const std::string cache_dir = get_cache_dir();
  const std::string path = join_path(cache_dir, "bad-next-update.crl");
  const time_t now = time(NULL);
  assert_true(write_test_crl(path, now - 3600, 0, true));
  assert_true(file_exists(path));

  {
    EnvOverride cache_dir_env("SF_CRL_RESPONSE_CACHE_DIR", cache_dir);
    EnvOverride delay_env("SF_CRL_ON_DISK_CACHE_REMOVAL_DELAY", "0");
    cleanupCertCRLCache();
  }

  assert_true(file_exists(path));
}

void test_no_crl_cache_if_disabled(void **unused) {
  SF_UNUSED(unused);

/*
 * CURLSSLOPT_NATIVE_CA should work with openssl on Windows only
 * https://curl.se/libcurl/c/CURLOPT_SSL_OPTIONS.html
 * but somehow it works on rhel so disable the test case on Ubuntu
 * when the required certificate is missing (in a different location
 * on Ubuntu).
 */
#ifdef __linux__
  if (access("/etc/pki/tls/certs/ca-bundle.crt", F_OK) != 0)
  {
    return;
  }
#endif

  const std::string cache_dir = get_cache_dir();

  CURL *ch = nullptr;
  {
    EnvOverride override("SF_CRL_RESPONSE_CACHE_DIR", cache_dir);
    assert_true(!dir_has_files(cache_dir));

    ch = curl_easy_init();
    assert_non_null(ch);

    assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_CHECK, 1L), CURLE_OK);
    assert_int_equal(curl_easy_setopt(ch, CURLOPT_SSL_SF_CRL_DISK_CACHING, 0L), CURLE_OK);
    curl_easy_setopt(ch, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(ch, CURLOPT_URL, "https://snowflake.com");

    CURLcode curlResult = curl_easy_perform(ch);
    assert_int_equal(curlResult, CURLE_OK);
  }

  curl_easy_cleanup(ch);
  // Check if any CRL has been downloaded
  assert_true(!dir_has_files(cache_dir));
}

int main() {
    EnvOverride cleanup_interval("SF_CRL_CACHE_CLEANUP_INTERVAL", "0");
    initialize_test(SF_BOOLEAN_FALSE);
    curl_global_init(CURL_GLOBAL_ALL);
    constexpr CMUnitTest tests[] = {
      cmocka_unit_test(test_success_with_crl_check),
      cmocka_unit_test(test_fail_with_no_crl),
      cmocka_unit_test(test_success_with_no_crl_if_allow_no_crl),
      cmocka_unit_test(test_success_with_no_crl_in_advisory_mode),
      cmocka_unit_test(test_curl_crl_params),
      cmocka_unit_test(test_crl_cache),
      cmocka_unit_test(test_no_crl_cache_if_disabled),
      cmocka_unit_test(test_crl_download_max_size_attribute),
      cmocka_unit_test(test_success_with_crl_check_custom_max_size),
      cmocka_unit_test(test_crl_cache_cleanup_removes_expired),
      cmocka_unit_test(test_crl_cache_cleanup_keeps_fresh),
      cmocka_unit_test(test_crl_cache_cleanup_removes_corrupt),
      cmocka_unit_test(test_crl_cache_cleanup_keeps_unparseable_next_update)
    };
    int ret = cmocka_run_group_tests(tests, nullptr, nullptr);
    snowflake_global_term();
    return ret;
}
