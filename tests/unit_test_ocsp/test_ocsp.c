/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 *
 * This test code is to validate HTTPS connection to the endpoint.
 *
 * This works only Linux at the moment.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>

extern CURLcode checkTelemetryHosts(char *hostname);

extern void get_cache_server_url(char* buf, size_t bufsize);

extern void get_cache_retry_url_pattern(char* buf, size_t bufsize);

extern CURLcode encodeUrlData(const char *url_data, size_t data_size, char** outptr, size_t *outlen);

struct configData
{
    char trace_ascii;
};

static void usage(char *program)
{
    printf("\nUsage: %s -h <host> -c cacert.pem [-p <port>]\n", program);
}

static size_t writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
    fwrite(ptr, size, nmemb, (FILE *) stream);
    return (nmemb * size);
}

static
void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size,
          char nohex)
{
    size_t i;
    size_t c;

    unsigned int width = 0x10;

    if (nohex)
        /* without the hex output, we can fit more on screen */
        width = 0x40;

    fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
            text, (long) size, (long) size);

    for (i = 0; i < size; i += width)
    {

        fprintf(stream, "%4.4lx: ", (long) i);

        if (!nohex)
        {
            /* hex not disabled, show it */
            for (c = 0; c < width; c++)
                if (i + c < size)
                    fprintf(stream, "%02x ", ptr[i + c]);
                else
                    fputs("   ", stream);
        }

        for (c = 0; (c < width) && (i + c < size); c++)
        {
            /* check for 0D0A; if found, skip past and start a new line of output */
            if (nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D &&
                ptr[i + c + 1] == 0x0A)
            {
                i += (c + 2 - width);
                break;
            }
            fprintf(stream, "%c",
                    (ptr[i + c] >= 0x20) && (ptr[i + c] < 0x80) ? ptr[i + c]
                                                                : '.');
            /* check again for 0D0A, to avoid an extra \n if it's at width */
            if (nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D &&
                ptr[i + c + 2] == 0x0A)
            {
                i += (c + 3 - width);
                break;
            }
        }
        fputc('\n', stream); /* newline */
    }
    fflush(stream);
}

static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
    struct configData *config = (struct configData *) userp;
    const char *text = NULL;
    (void) handle; /* prevent compiler warning */

    switch (type)
    {
        case CURLINFO_TEXT:
            fprintf(stderr, "== Info: %s", data);
            /* FALLTHROUGH */
        default: /* in case a new one is introduced to shock us */
            return 0;

        case CURLINFO_HEADER_OUT:
            //text = "=> Send header";
            break;
        case CURLINFO_DATA_OUT:
            // text = "=> Send data";
            break;
        case CURLINFO_SSL_DATA_OUT:
            // text = "=> Send SSL data";
            break;
        case CURLINFO_HEADER_IN:
            //text = "<= Recv header";
            break;
        case CURLINFO_DATA_IN:
            // text = "<= Recv data";
            break;
        case CURLINFO_SSL_DATA_IN:
            // text = "<= Recv SSL data";
            break;
    }

    if (text != NULL)
    {
        dump(text, stderr, (unsigned char *) data, size, config->trace_ascii);
    }
    return 0;
}

static void dieIfNotSuccess(CURLcode ret)
{
    if (ret != CURLE_OK)
    {
        fprintf(stderr, "FAILED!\n");
        exit(1);
    }
}

static void
checkCertificateRevocationStatus(char *host, char *port, char *cacert, char *proxy, char *no_proxy, int oob_enable, int failopen, int expect_fail)
{
    CURL *ch;
    struct configData config;
    char url[4096];

    config.trace_ascii = 1;

    sprintf(url, "https://%s:%s/", host, port);

    dieIfNotSuccess(curl_global_init(CURL_GLOBAL_ALL));
    ch = curl_easy_init();
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_HEADER, 0L));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1L));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1L));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, &writefunction));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_WRITEDATA, stdout));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, &writefunction));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_HEADERDATA, stderr));

    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_DEBUGFUNCTION, my_trace));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_DEBUGDATA, &config));

    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 1L));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_URL, url));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_CAINFO, cacert));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_CAPATH, NULL));
    dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_SSL_SF_OCSP_CHECK, 1));

    if (proxy)
    {
        dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_PROXY, proxy));
    }
    if (no_proxy)
    {
        dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_NOPROXY, no_proxy));
    }

    if (oob_enable != 0)
    {
        dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_SSL_SF_OOB_ENABLE, 1));
    }
    else
    {
        dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_SSL_SF_OOB_ENABLE, 0));
    }

    if (failopen != 0)
    {
        dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_SSL_SF_OCSP_FAIL_OPEN, 1));
    }
    else
    {
        dieIfNotSuccess(curl_easy_setopt(ch, CURLOPT_SSL_SF_OCSP_FAIL_OPEN, 0));
    }

    CURLcode ret = curl_easy_perform(ch);
    if (expect_fail == 0)
    {
        dieIfNotSuccess(ret);
    }
    else
    {
        if (ret == CURLE_OK)
        {
            fprintf(stderr, "FAILED!\n");
            exit(1);
        }        
    }

    curl_easy_cleanup(ch);
    curl_global_cleanup();
    fprintf(stderr, "OK\n");
}

static void
checkUrlEncoding(char *urlData, char *expectedEncoding)
{
    char *encodedData;

    encodeUrlData(urlData, strlen(urlData), &encodedData, NULL);
    if (strcmp(encodedData, expectedEncoding) != 0)
    {
        fprintf(stderr, "checkUrlEncoding FAILED! expected %s but actually returned %s\n",
                expectedEncoding, encodedData);
        free(encodedData);
        exit(1);
    }

    fprintf(stderr, "OK\n");
}

static void
checkDefaultURLDomain(char *host, char *port, char *cacert)
{
// Max length of buffer
#define MAX_BUFFER_LENGTH 4096
    char expectedCacheServerURL[MAX_BUFFER_LENGTH];
    char expectedRetryURL[MAX_BUFFER_LENGTH];
    char buf[MAX_BUFFER_LENGTH];

    // unset environment variables to use default behavior
    unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED");
    unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");
    // test default URL being used when ACTIVATE_SSD is on
    setenv("SF_OCSP_ACTIVATE_SSD", "true", 1);

    char * domain = strrchr(host, '.') + 1;
    sprintf(expectedCacheServerURL, "http://ocsp.snowflakecomputing.%s/ocsp_response_cache.json", domain);
    sprintf(expectedRetryURL, "http://ocsp.snowflakecomputing.%s/retry", domain);

    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 0, 0, 0);

    get_cache_server_url(buf, sizeof(buf));
    if (strcmp(buf, expectedCacheServerURL) != 0)
    {
        fprintf(stderr, "checkDefaultURLDomain FAILED! expected cache server URL %s but actually %s\n",
                expectedCacheServerURL, buf);
        exit(1);
    }

    get_cache_retry_url_pattern(buf, sizeof(buf));
    if (strcmp(buf, expectedRetryURL) != 0)
    {
        fprintf(stderr, "checkDefaultURLDomain FAILED! expected retry URL %s but actually %s\n",
                expectedRetryURL, buf);
        exit(1);
    }

    unsetenv("SF_OCSP_ACTIVATE_SSD");
    fprintf(stderr, "OK\n");
}

int main(int argc, char **argv)
{
    char cacert[4096];
    char host[4096];
    char port[100] = "443";
    int c;

    char cache_file[4096];

    host[0] = (char) 0;
    if (getenv("SNOWFLAKE_TEST_HOST"))
    {
        strcpy(host, getenv("SNOWFLAKE_TEST_HOST"));
    }
    else if (getenv("SNOWFLAKE_TEST_ACCOUNT"))
    {
        sprintf(host, "%s.snowflakecomputing.com", getenv("SNOWFLAKE_TEST_ACCOUNT"));
    }
    cacert[0] = (char) 0;
    if (getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE"))
    {
        strcpy(cacert, getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE"));
    }

    while ((c = getopt(argc, argv, "h:p:c:")) != -1)
    {
        switch (c)
        {
            case 'h':
                strcpy(host, optarg);
                break;
            case 'c':
                strcpy(cacert, optarg);
                break;
            case 'p':
                strcpy(port, optarg);
                break;
            default:
                break;
        }
    }
    if (strlen(host) == 0 || strlen(cacert) == 0)
    {
        usage(argv[0]);
        return 2;
    }
    printf("host: %s, port: %s, cacert: %s\n", host, port, cacert);
#ifdef __linux__
    sprintf(cache_file, "%s/.cache/snowflake/ocsp_response_cache.json",
            getenv("HOME"));
#elif defined(__APPLE__)
    sprintf(cache_file, "%s/Library/Caches//Snowflake/ocsp_response_cache.json",
            getenv("HOME"));
#else
    return 0;
#endif

    printf("===> Case 1: whatever default\n");
    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 0, 0, 0);

    printf("===> Case 2: Delete file cache and No Use Cache Server\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false", 1);
    unlink(cache_file);
    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 0, 0, 0);

    printf("===> Case 3: Delete file cache and Use Cache Server\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "true", 1);
    unlink(cache_file);
    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 0, 0, 0);

    printf("===> Case 4: No Delete file cache and No Use Cache Server\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false", 1);
    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 0, 0, 0);

    printf("===> Case 5: No Delete file cache and No Use Cache Server\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false", 1);
    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 0, 0, 0);

    if (getenv("all_proxy") || getenv("https_proxy") ||
        getenv("http_proxy"))
    {
        // skip the test if the test evironment uses proxy already
        return 0;
    }

    printf("===> Case 6: Delete file cache and overwrite invalid proxy in env\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "true", 1);
    setenv("http_proxy", "a.b.c", 1);
    setenv("https_proxy", "a.b.c", 1);
    unlink(cache_file);
    checkCertificateRevocationStatus(host, port, cacert, "", "", 0, 0, 0);

    printf("===> Case 7: Delete file cache and overwrite invalid proxy with no_proxy\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "true", 1);
    setenv("http_proxy", "a.b.c", 1);
    setenv("https_proxy", "a.b.c", 1);
    unlink(cache_file);
    checkCertificateRevocationStatus(host, port, cacert, "a.b.c", "*", 0, 0, 0);

    unsetenv("http_proxy");
    unsetenv("https_proxy");

    printf("===> Case 8: Check URL encoding for OCSP request\n");
    checkUrlEncoding("Hello @World", "Hello%20%40World");
    checkUrlEncoding("Test//String", "Test%2F%2FString");

    printf("===> Case 9: Delete file cache with invalid cache server URL to test OOB enabled\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false", 1);
    // use random IP address so it will get connection timeout
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL", "http://10.24.123.89/ocsp_response_cache.json", 1);
    unlink(cache_file);
    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 1, 1, 0);

    printf("===> Case 10: Delete file cache with invalid cache server URL to test delay on failure and OOB disabled\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false", 1);
    // use random IP address so it will get connection timeout
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL", "http://10.24.123.89/ocsp_response_cache.json", 1);
    unlink(cache_file);

    time_t start_time = time(NULL);
    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 0, 1, 0);
    time_t end_time = time(NULL);
    // should be around 5 seconds but no longer than 10.
    if ((end_time - start_time) > 10)
    {
        fprintf(stderr, "Delay check FAILED! Delayed %ld seconds\n", end_time - start_time);
        exit(1);
    }
    else
    {
        fprintf(stderr, "Delay check OK\n");
    }

    printf("===> Case 11: Delete file cache with invalid cache server URL test with fail close\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "true", 1);
    // use random IP address so it will get connection timeout
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL", "http://10.24.123.89/ocsp_response_cache.json", 1);
    unlink(cache_file);

    checkCertificateRevocationStatus(host, port, cacert, NULL, NULL, 0, 0, 1);

    unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED");
    unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");

    printf("===> Case 12: Ignore top level domain when checking telemetry endpoints\n");
    dieIfNotSuccess(checkTelemetryHosts("sfctest.client-telemetry.snowflakecomputing.com"));
    dieIfNotSuccess(checkTelemetryHosts("sfcdev.client-telemetry.snowflakecomputing.cn"));
    dieIfNotSuccess(checkTelemetryHosts("client-telemetry.snowflakecomputing.mil"));
    fprintf(stderr, "OK\n");

    printf("===> Case 13: Top level domain used in default OCSP URLs\n");
    checkDefaultURLDomain(host, port, cacert);
    return 0;
}
