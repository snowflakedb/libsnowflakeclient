/*
 * File:   secure_storage.cpp *
 */

#include <cstring>
#include <map>
#include <string>

#include "snowflake/secure_storage.h"
#include "snowflake/SecureStorage.hpp"

namespace {
  // Copies a std::string into a heap buffer owned by the caller, freed via
  // secure_storage_free_credential.
  char* dupCredential(const std::string& token)
  {
    size_t result_size = token.size() + 1;
    char* result = new char[result_size];
    strncpy(result, token.c_str(), result_size);
    return result;
  }

  const char* orEmpty(const char* s) { return s ? s : ""; }
}

#ifdef __cplusplus
extern "C" {
  using namespace Snowflake::Client;
#endif

secure_storage_ptr secure_storage_init() {
    return new Snowflake::Client::SecureStorage();
}

char* secure_storage_get_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type)
{
  return secure_storage_get_credential_v2(tc, host, user, type, host, host, "");
}

char* secure_storage_get_credential_v2(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char* idp, const char* snowflake, const char* role)
{
  Snowflake::Client::SecureStorageKey key(orEmpty(host), orEmpty(user), type, orEmpty(idp), orEmpty(snowflake), orEmpty(role));
  std::string token;
  auto status = reinterpret_cast<Snowflake::Client::SecureStorage *>(tc)->retrieveToken(key, token);
  if (status != SecureStorageStatus::Success)
  {
      return nullptr;
  }
  return dupCredential(token);
}

void secure_storage_free_credential(char* cred) {
    delete[] cred;
}

bool secure_storage_save_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char *cred)
{
  return secure_storage_save_credential_v2(tc, host, user, type, host, host, "", cred);
}

bool secure_storage_save_credential_v2(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char* idp, const char* snowflake, const char* role, const char *cred)
{
  Snowflake::Client::SecureStorageKey key(orEmpty(host), orEmpty(user), type, orEmpty(idp), orEmpty(snowflake), orEmpty(role));
  return reinterpret_cast<Snowflake::Client::SecureStorage *>(tc)->storeToken(key, std::string(cred)) == SecureStorageStatus::Success;
}

bool secure_storage_remove_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type)
{
  return secure_storage_remove_credential_v2(tc, host, user, type, host, host, "");
}

bool secure_storage_remove_credential_v2(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char* idp, const char* snowflake, const char* role)
{
  Snowflake::Client::SecureStorageKey key(orEmpty(host), orEmpty(user), type, orEmpty(idp), orEmpty(snowflake), orEmpty(role));
  return reinterpret_cast<Snowflake::Client::SecureStorage *>(tc)->removeToken(key) == SecureStorageStatus::Success;
}

void secure_storage_term(secure_storage_ptr tc) {
    delete reinterpret_cast<Snowflake::Client::SecureStorage*>(tc);
}

#ifdef __cplusplus
};
#endif
