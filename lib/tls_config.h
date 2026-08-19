#ifndef SNOWFLAKE_TLS_CONFIG_H
#define SNOWFLAKE_TLS_CONFIG_H

#include <curl/curl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set CURLOPT_SSLVERSION on handle. tlsVersion is a CURL_SSLVERSION_* value,
 * or SF_TLS_VERSION_UNSET (-1) for a no-op. Touches no other option. */
CURLcode sf_apply_tls_version(CURL* handle, int tlsVersion);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_TLS_CONFIG_H
