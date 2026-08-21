/* Shared TLS-version helper. See tls_config.h. */

#include "tls_config.h"
#include <snowflake/client.h>   /* SF_TLS_VERSION_UNSET */

CURLcode sf_apply_tls_version(CURL* handle, int tlsVersion)
{
    if (!handle || tlsVersion == SF_TLS_VERSION_UNSET)
    {
        return CURLE_OK;
    }
    /* CURLOPT_SSLVERSION reads a long via varargs. */
    return curl_easy_setopt(handle, CURLOPT_SSLVERSION, (long)tlsVersion);
}

int sf_resolve_tls_version(int tlsVersion)
{
    int32 global = SF_TLS_VERSION_UNSET;
    if (tlsVersion != SF_TLS_VERSION_UNSET)
    {
        return tlsVersion;
    }
    snowflake_global_get_attribute(SF_GLOBAL_SSL_VERSION, &global, sizeof(global));
    return (int)global;
}
