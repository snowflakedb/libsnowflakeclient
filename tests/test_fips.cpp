#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <functional>
#include <memory>
#include <boost/filesystem.hpp>
#include "snowflake/IBase64.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"


bool fips_setup()
{
  // relevant paths for command line
  std::string openssl_root = std::string("openssl");
  std::string openssl_bin = openssl_root + PATH_SEP + "bin" + PATH_SEP + "openssl";
  std::string openssl_modules = openssl_root + PATH_SEP + "lib";
#ifndef _WIN32
  openssl_modules += PATH_SEP;
  openssl_modules += "ossl-modules";
#endif
  std::string fips_module = openssl_modules + PATH_SEP + "fips";
#if defined(_WIN32)
  fips_module += ".dll";
#elif defined(__linux__)
  fips_module += ".so";
#else
  fips_module += ".dylib";
#endif
  // absolute paths for environment variables
  std::string current_path = boost::filesystem::current_path().string();
  std::string openssl_conf = current_path + PATH_SEP + openssl_root + PATH_SEP + "openssl.cnf";
  std::string openssl_include = current_path + PATH_SEP + openssl_root;
  openssl_modules = current_path + PATH_SEP + openssl_modules;

  std::string fips_install_cmd = openssl_bin + " fipsinstall" +
    " -module " + fips_module +
    " -out " + openssl_root + PATH_SEP + "fipsmodule.cnf";
  int res = system(fips_install_cmd.c_str());
  if (res != 0)
  {
    return false;
  }

  sf_setenv("OPENSSL_CONF", openssl_conf.c_str());
  sf_setenv("OPENSSL_CONF_INCLUDE", openssl_include.c_str());
  sf_setenv("OPENSSL_MODULES", openssl_modules.c_str());
  return true;
}

void test_fips_enabled(void **unused) {
  sf_bool fips_enabled = SF_BOOLEAN_FALSE;
  snowflake_global_get_attribute(SF_GLOBAL_FIPS_ENABLED, &fips_enabled, 0);
  assert_int_equal(fips_enabled, SF_BOOLEAN_TRUE);

  SF_CONNECT *sf = setup_snowflake_connection();

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);

  /* Set query result format to Arrow if necessary */
  status = snowflake_query(sfstmt, "select 1", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

int main(void) {
  if (!fips_setup()) {
    printf("Skipping - fips configuration failed\n");
    return 0;
  }

  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_fips_enabled),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
