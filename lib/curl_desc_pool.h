#ifndef SNOWFLAKE_CURL_DESC_POOL_H
#define SNOWFLAKE_CURL_DESC_POOL_H

#include <curl/curl.h>
#include <snowflake/basic_types.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Get curl desc instance from pool.
     *
     * Sub-pools are partitioned by TLS version in addition to endpoint/proxy so
     * that a pooled handle (whose live connection has a TLS version fixed at
     * handshake) is only ever reused for a request with the same TLS version.
     *
     * @param url          The url of the rest request
     * @param proxy        The proxy setting, null if not available.
     * @param no_proxy     The proxy setting, null if not available.
     * @param tls_version  Per-connection TLS version (a CURL_SSLVERSION_* value),
     *                     or SF_TLS_VERSION_UNSET (-1) for no override.
     *
     * @return curl desc instance from pool
     */

    void* get_curl_desc_from_pool(const char* url, const char* proxy, const char* no_proxy, int32 tls_version);

    /**
     * Get curl handle from the curl description returned from get_curl_desc_from_pool().
     *
     * @param curl_desc The curl desc instance returned from get_curl_desc_from_pool().
     *
     * @return curl     The curl handle in the desc instance.
     */
    CURL* get_curl_from_desc(void * curl_desc);

    /**
     * Free curl desc instance and return it to pool
     *
     * @param curl_desc  The curl desc instance returned from get_curl_desc_from_pool().
     *
     */
    void free_curl_desc(void * curl_desc);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_CURL_DESC_POOL_H
