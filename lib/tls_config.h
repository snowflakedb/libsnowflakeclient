#ifndef SNOWFLAKE_TLS_CONFIG_H
#define SNOWFLAKE_TLS_CONFIG_H

/*
 * Shared TLS-version configuration helper.
 *
 * Used by both the C core (lib/http_perform.c) and the C++ AWS custom curl
 * client (cpp/AwsTlsHttpClient.cpp) so the actual CURLOPT_SSLVERSION is set in
 * exactly one place.
 *
 * Values are libcurl CURL_SSLVERSION_* constants. The "not set" sentinel is
 * SF_TLS_VERSION_UNSET (-1), defined in <snowflake/client.h>; when a caller
 * passes it, sf_apply_tls_version() is a no-op and the caller's own fallback
 * (session -> global -> library default) decides what to apply.
 */

#include <curl/curl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Apply a TLS version to a curl handle. Touches ONLY CURLOPT_SSLVERSION; it
 * never changes any other curl option.
 *
 * @param handle          curl easy handle.
 * @param tlsVersion      A CURL_SSLVERSION_* value (optionally OR'd with a
 *                        CURL_SSLVERSION_MAX_* bit for exact pinning), or
 *                        SF_TLS_VERSION_UNSET to do nothing.
 * @return CURLE_OK on success, otherwise the failing CURLcode.
 */
CURLcode sf_apply_tls_version(CURL* handle, long tlsVersion);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_TLS_CONFIG_H
