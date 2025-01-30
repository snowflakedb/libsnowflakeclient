/*
 * File:   secure_storage.cpp *
 * Copyright (c) 2025 Snowflake Computing
 */

#include <cstring>
#include <map>

#include "snowflake/secure_storage.h"
#include "snowflake/SecureStorage.hpp"

#ifdef __cplusplus
extern "C" {
  using namespace Snowflake::Client;
#endif

secure_storage_ptr secure_storage_init() {
    return new Snowflake::Client::SecureStorage();
}

char* secure_storage_get_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type)
{
  Snowflake::Client::SecureStorageKey key = { host, user, type };
  std::string token;
  auto status = reinterpret_cast<Snowflake::Client::SecureStorage *>(tc)->retrieveToken(key, token);
  if (status != SecureStorageStatus::Success)
  {
      return nullptr;
  }
  size_t result_size = token.size() + 1;
  char* result = new char[result_size];
  strncpy(result, token.c_str(), result_size);
  return result;
}

void secure_storage_free_credential(char* cred) {
    delete[] cred;
}

void secure_storage_save_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char *cred)
{
  Snowflake::Client::SecureStorageKey key = { host, user, type };
  reinterpret_cast<Snowflake::Client::SecureStorage *>(tc)->storeToken(key, std::string(cred));
}

void secure_storage_remove_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type)
{
  Snowflake::Client::SecureStorageKey key = { host, user, type };
  reinterpret_cast<Snowflake::Client::SecureStorage *>(tc)->removeToken(key);
}

void secure_storage_term(secure_storage_ptr tc) {
    delete reinterpret_cast<Snowflake::Client::SecureStorage*>(tc);
}

#ifdef __cplusplus
};
#endif

