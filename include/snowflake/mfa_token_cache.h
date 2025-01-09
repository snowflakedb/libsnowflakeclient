//
// Created by Jakub Szczerbinski on 10.10.24.
//

#ifndef SNOWFLAKECLIENT_MFA_TOKEN_CACHE_H
#define SNOWFLAKECLIENT_MFA_TOKEN_CACHE_H

typedef void* cred_cache_ptr;

typedef enum {
  MFA_TOKEN
} CredentialType;

#ifdef __cplusplus
extern "C" {
#endif

cred_cache_ptr cred_cache_init();
char* cred_cache_get_credential(cred_cache_ptr tc, const char* host, const char* user, CredentialType type);
void cred_cache_free_credential(char* cred);
void cred_cache_save_credential(cred_cache_ptr tc, const char* host, const char* user, CredentialType type, const char *cred);
void cred_cache_remove_credential(cred_cache_ptr tc, const char* host, const char* user, CredentialType type);
void cred_cache_term(cred_cache_ptr tc);

#ifdef __cplusplus
};
#endif

#endif //SNOWFLAKECLIENT_MFA_TOKEN_CACHE_H
