#ifndef SNOWFLAKE_CURL_DESC_POOL_H
#define SNOWFLAKE_CURL_DESC_POOL_H

#include <curl/curl.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Get curl desc instance from pool.
     *
     * @param url       The url of the rest request
     * @param proxy     The proxy setting, null if not available.
     * @param no_proxy  The proxy setting, null if not available.
     *
     * @return curl desc instance from pool
     */
    void* get_curl_desc_from_pool(const char* url, const char* proxy, const char* no_proxy);

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
