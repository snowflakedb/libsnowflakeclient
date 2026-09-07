/*
 * File:   secure_storage.h *
 */


#ifndef SNOWFLAKECLIENT_SECURE_STORAGE_H
#define SNOWFLAKECLIENT_SECURE_STORAGE_H

#include <stdbool.h>

typedef void* secure_storage_ptr;

typedef enum {
  MFA_TOKEN,
  ID_TOKEN,
  OAUTH_REFRESH_TOKEN,
  OAUTH_ACCESS_TOKEN,
  DPOP_BUNDLED_ACCESS_TOKEN
} SecureStorageKeyType;

#ifdef __cplusplus
extern "C" {
#endif

secure_storage_ptr secure_storage_init();

/*
 * Backward-compatible accessors. The cache key is derived from a canonical-JSON
 * document (see SecureStorage::convertTarget). These wrappers set idp == snowflake
 * == host and leave the role empty.  For MFA_TOKEN and ID_TOKEN the idp/role
 * dimensions are not part of the key (only snowflake + username matter), so
 * these wrappers produce identical results to the *_v2 variants for those types.
 * Only OAuth flows require non-trivial idp and role values; the *_v2 variants
 * below MUST be used for OAUTH_* and DPOP_* token types.
 */
char* secure_storage_get_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type);
void secure_storage_free_credential(char* cred);
bool secure_storage_save_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char *cred);
bool secure_storage_remove_credential(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type);

/*
 * v2 accessors that carry the full set of key dimensions. `idp` is the IdP /
 * token-endpoint URL, `snowflake` is the Snowflake server URL, and `role` is the
 * session role (may be empty). `host` is retained for logging only and is not
 * part of the key. Any of `idp`, `snowflake`, or `role` may be NULL (treated as
 * empty).
 */
char* secure_storage_get_credential_v2(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char* idp, const char* snowflake, const char* role);
bool secure_storage_save_credential_v2(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char* idp, const char* snowflake, const char* role, const char *cred);
bool secure_storage_remove_credential_v2(secure_storage_ptr tc, const char* host, const char* user, SecureStorageKeyType type, const char* idp, const char* snowflake, const char* role);

void secure_storage_term(secure_storage_ptr tc);

#ifdef __cplusplus
};
#endif

#endif //SNOWFLAKECLIENT_SECURE_STORAGE_H
