/*
 * Shared TLS-version configuration helper. See tls_config.h.
 *
 * Plain C so both the C core (lib/http_perform.c) and the C++ AWS custom curl
 * client (cpp/AwsTlsHttpClient.cpp) can call it; the header declares it with C
 * linkage under C++.
 */

#include "tls_config.h"
#include <snowflake/client.h>   /* SF_TLS_VERSION_UNSET */

CURLcode sf_apply_tls_version(CURL* handle, long tlsVersion)
{
    /* SF_TLS_VERSION_UNSET => no override; caller's fallback decides. */
    if (!handle || tlsVersion == SF_TLS_VERSION_UNSET)
    {
        return CURLE_OK;
    }

    return curl_easy_setopt(handle, CURLOPT_SSLVERSION, tlsVersion);
}
