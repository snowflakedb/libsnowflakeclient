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
checkCertificateRevocationStatus(char *host, char *port, char *cacert)
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

    dieIfNotSuccess(curl_easy_perform(ch));

    curl_easy_cleanup(ch);
    curl_global_cleanup();
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
    cacert[0] = (char) 0;

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
    sprintf(cache_file, "%s/.cache/snowflake/ocsp_response_cache.json",
            getenv("HOME"));

    printf("===> Case 1: whatever default\n");
    checkCertificateRevocationStatus(host, port, cacert);

    printf("===> Case 2: Delete file cache and No Use Cache Server\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false", 1);
    unlink(cache_file);
    checkCertificateRevocationStatus(host, port, cacert);

    printf("===> Case 3: Delete file cache and Use Cache Server\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "true", 1);
    unlink(cache_file);
    checkCertificateRevocationStatus(host, port, cacert);

    printf("===> Case 4: No Delete file cache and No Use Cache Server\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false", 1);
    checkCertificateRevocationStatus(host, port, cacert);

    printf("===> Case 5: No Delete file cache and No Use Cache Server\n");
    setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false", 1);
    checkCertificateRevocationStatus(host, port, cacert);

    return 0;
}

