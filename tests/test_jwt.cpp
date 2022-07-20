/*
 * Copyright (c) 2022 Snowflake Computing, Inc. All rights reserved.
 */

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <functional>
#include <memory>
#include "snowflake/IBase64.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"


/**
 * Helper class to generate and serialize private/public key pair
 */
class KeyPairHolder
{

public:
  /**
   * Constructor that generate random RSA key
   */
  KeyPairHolder()
  {
    EVP_PKEY *key = nullptr;
    std::unique_ptr<EVP_PKEY_CTX, std::function<void(EVP_PKEY_CTX *)>> kct
        {EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), [](EVP_PKEY_CTX *ctx)
        { if (ctx) EVP_PKEY_CTX_free(ctx); }};

    if (EVP_PKEY_keygen_init(kct.get()) <= 0) throw std::exception();

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(kct.get(), 2048) <= 0) throw std::exception();

    if (EVP_PKEY_keygen(kct.get(), &key) <= 0) throw std::exception();

    m_key = {key, EVP_PKEY_deletor};
  }

  /**
   * Constructor that load RSA key from file
   */
  KeyPairHolder(const std::string &fileName, const std::string &passcode = "")
  {
    FILE *file = fopen(fileName.c_str(), "r");
    if (file == nullptr)
    {
      throw std::exception();
    }

    EVP_PKEY *privKey = PEM_read_PrivateKey(file, nullptr, nullptr, (void *)passcode.c_str());

    if (privKey == nullptr) {
      fclose(file);
      throw std::exception();
    }
    m_key = {privKey, EVP_PKEY_deletor};
  }

  /**
   * get the public key bytes encoded in SPKI format
   */
  std::vector<char> getPubBytes()
  {
    unsigned char *out = nullptr;
    int size = i2d_PUBKEY(m_key.get(), &out);
    if (size < 0) throw std::exception();

    std::vector<char> pubKeyBytes(out, out + size);
    OPENSSL_free(out);
    return pubKeyBytes;
  }

  /**
   * get the public key bytes encoded in PKCS8 format
   */
  std::vector<char> getPrivBytes()
  {
    unsigned char *out = nullptr;
    int size = i2d_PrivateKey(m_key.get(), &out);
    if (size < 0) throw std::exception();

    std::vector<char> privKeyBytes(out, out + size);
    OPENSSL_free(out);
    return privKeyBytes;
  }

  inline EVP_PKEY *getEvpKey()
  { return m_key.get(); }

  /**
   * Get the generated key pair
   * @return
   */
  inline EVP_PKEY *getKeyPair() {
    return m_key.get();
  }

  void saveUnencryptedPrivateKey(const std::string& path)
  {
    FILE *f;
    f = fopen(path.c_str(), "w+");
    if (!f)
    {
      return;
    }
    PEM_write_PrivateKey(f, m_key.get(), NULL, NULL, 0, 0, NULL);
    fclose(f);
  }

  void saveEncryptedPrivateKey(const std::string& path, const std::string& pwd)
  {
    FILE *f;
    f = fopen(path.c_str(), "w+");
    if (!f)
    {
      return;
    }
    PEM_write_PrivateKey(f, m_key.get(), EVP_des_ede3_cbc(), (unsigned char*)pwd.c_str(), pwd.length(), NULL, NULL);
    fclose(f);
  }

typedef std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY *)>> EVP_PKEY_uptr;

private:
  static inline void EVP_PKEY_deletor(EVP_PKEY *key)
  {
    if (key)  EVP_PKEY_free(key);
  }
  EVP_PKEY_uptr m_key;

};

static void loadPublicKey(KeyPairHolder &keyPairHolder) {
  SF_CONNECT *sf = setup_snowflake_connection();

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  auto pubKey = Snowflake::Client::Util::IBase64::encodePadding(keyPairHolder.getPubBytes());

  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_query(sfstmt, "use role accountadmin;", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  status = snowflake_query(sfstmt, "set username=CURRENT_USER();", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  std::string load_stmt(std::string("ALTER USER identifier($username) SET rsa_public_key='") + pubKey + "';");
  status = snowflake_query(sfstmt, load_stmt.c_str(), 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  snowflake_term(sf);
}

void test_missing_private_key(void **unused) {
  SF_CONNECT *sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_JWT);

  SF_STATUS status = snowflake_connect(sf);
  assert_int_not_equal(status, SF_STATUS_SUCCESS);
  SF_ERROR_STRUCT *error = snowflake_error(sf);
  assert_int_equal(error->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
  snowflake_term(sf);

  sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_JWT);
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE, "invalid_key_file");

  status = snowflake_connect(sf);
  assert_int_not_equal(status, SF_STATUS_SUCCESS);
  error = snowflake_error(sf);
  assert_int_equal(error->error_code, SF_STATUS_ERROR_GENERAL);
  snowflake_term(sf);
}

void test_unencrypted_pem(void **unused) {
  std::string dataDir = TestSetup::getDataDir();
  std::string keyFilePath = dataDir + "p1.pem";
  KeyPairHolder keypairHolder;
  keypairHolder.saveUnencryptedPrivateKey(keyFilePath);
  loadPublicKey(keypairHolder);

  SF_CONNECT *sf = setup_snowflake_connection();

  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_JWT);
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE, keyFilePath.c_str());

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  remove(keyFilePath.c_str());
}

void test_encrypted_pem(void **unused) {
  std::string dataDir = TestSetup::getDataDir();
  std::string keyFilePath = dataDir + "p1.pem";
  KeyPairHolder keypairHolder;
  keypairHolder.saveEncryptedPrivateKey(keyFilePath, "test");
  loadPublicKey(keypairHolder);

  SF_CONNECT *sf = setup_snowflake_connection();

  // connect with correct key file and password
  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_JWT);
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE, keyFilePath.c_str());
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE_PWD, "test");

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  // connect error with no password
  sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_JWT);
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE, keyFilePath.c_str());

  status = snowflake_connect(sf);
  assert_int_not_equal(status, SF_STATUS_SUCCESS);
  SF_ERROR_STRUCT *error = snowflake_error(sf);
  assert_int_equal(error->error_code, SF_STATUS_ERROR_GENERAL);
  snowflake_term(sf);

  // connect error with empty password
  sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_JWT);
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE, keyFilePath.c_str());
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE_PWD, "");

  status = snowflake_connect(sf);
  assert_int_not_equal(status, SF_STATUS_SUCCESS);
  error = snowflake_error(sf);
  assert_int_equal(error->error_code, SF_STATUS_ERROR_GENERAL);
  snowflake_term(sf);

  // connect error with invalid password
  sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_JWT);
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE, keyFilePath.c_str());
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE_PWD, "invalid pwd");

  status = snowflake_connect(sf);
  assert_int_not_equal(status, SF_STATUS_SUCCESS);
  error = snowflake_error(sf);
  assert_int_equal(error->error_code, SF_STATUS_ERROR_GENERAL);
  snowflake_term(sf);

  remove(keyFilePath.c_str());
}

void test_renew(void **unused) {
  std::string dataDir = TestSetup::getDataDir();
  std::string keyFilePath = dataDir + "p1.pem";
  KeyPairHolder keypairHolder;
  keypairHolder.saveUnencryptedPrivateKey(keyFilePath);
  loadPublicKey(keypairHolder);

  SF_CONNECT *sf = setup_snowflake_connection();

  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_JWT);
  snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE, keyFilePath.c_str());
  int64 renew_timeout = 5;
  snowflake_set_attribute(sf, SF_CON_JWT_CNXN_WAIT_TIME, &renew_timeout);
  snowflake_set_attribute(sf, SF_CON_PASSWORD, "renew injection");

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  remove(keyFilePath.c_str());
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_missing_private_key),
    cmocka_unit_test(test_unencrypted_pem),
    cmocka_unit_test(test_encrypted_pem),
    cmocka_unit_test(test_renew),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
