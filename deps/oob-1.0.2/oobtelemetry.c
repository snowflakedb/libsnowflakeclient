#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "snowflake/Simba_CRTFunctionSafe.h"
#include "oobtelemetry.h"

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;

    /*Response from server like {"statusCode": 200, "message": "SUCCESS"} are received here.
     * printf("Contents are %s\n",(char*)contents);
     */

    return realsize;
}

#define NUM_END_POINTS 3

struct WriteThis {
    const char *readptr;
    size_t sizeleft;
};

typedef struct {
    const char *name;
    const char *enqendpoint;
    const char *apikey;
    char *deployment;
} endPoint;

static endPoint endPoints[] = {
        {
                "sfc-test",
                "https://sfctest.client-telemetry.snowflakecomputing.com/enqueue",
                "x-api-key: rRNY3EPNsB4U89XYuqsZKa7TSxb9QVX93yNM4tS6",
                "dev"
        },
        {
                "sfc-dev",
                "https://sfcdev.client-telemetry.snowflakecomputing.com/enqueue",
                "x-api-key: kyTKLWpEZSaJnrzTZ63I96QXZHKsgfqbaGmAaIWf",
                "qa1"
        },
        {
                "sfc-va-prod",
                "https://client-telemetry.snowflakecomputing.com/enqueue",
                "x-api-key: wLpEKqnLOW9tGNwTjab5N611YQApOb3t9xOnE1rX",
                "Production"
        }
};

/*param event is the stringified json oob event with deployment information.
 * "telemetryServerDeployment" : "dev"
 * Retrieves the deployment type from the string.
 * dep is the preallocated variable for deployment.
 */
void getdeploymenttype(const char *event, char *dep, int depSize) {
    int len = 0;
    const char *depStart = NULL;
    const char *del = NULL;

    if (event == NULL || event[0] == 0) return;

    depStart = strstr(event, "telemetryServerDeployment");
    if (depStart) {
        del = strstr(depStart, ":");
        depStart = strstr(del, "\"");
        del = strstr(depStart + 1, "\"");
        len = (int) (del - depStart - 1);
        len = (depSize - 1 < len) ? depSize - 1 : len;
        sb_strncpy(dep, depSize, depStart + 1, len);
    }
    dep[len] = 0;
    return;
}

/*Default endpoint is production deplyment*/
endPoint *getendPoint(const char *event) {
    int i = 0;
    char deployment[25] = {0};
    getdeploymenttype(event, deployment, 25);
    if (strcmp(deployment, "Ignore") == 0 || deployment[0] == 0) {
        //If the host information is NULL we ignore such OOB messages.
        return NULL;
    }

    for (i = 0; i < NUM_END_POINTS; i++) {
        //haystack needle search with case insensitive
        if (strstr(endPoints[i].deployment, deployment) != NULL) {
            return &(endPoints[i]);
        }
    }

    return &(endPoints[2]);
}

static size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp) {
    struct WriteThis *wt = (struct WriteThis *) userp;
    size_t buffer_size = size * nmemb;

    if (wt->sizeleft) {
        /* copy as much as possible from the source to the destination */
        size_t copy_this_much = wt->sizeleft;
        if (copy_this_much > buffer_size)
            copy_this_much = buffer_size;
        sb_memcpy(dest, buffer_size, wt->readptr, copy_this_much);

        wt->readptr += copy_this_much;
        wt->sizeleft -= copy_this_much;
        return copy_this_much; /* we copied this many bytes */
    }

    return 0; /* no more data left to deliver */
}

static void cleanup(CURL *curl, struct curl_slist *headers) {
    if (curl) {
        curl_easy_cleanup(curl);
    }
    curl_slist_free_all(headers);
    curl_global_cleanup();
}

int sendOOBevent(char *event) {
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    CURLcode res;
    struct WriteThis wt;
    endPoint *ep;
    char caBundle[512] = {0};

    ep = getendPoint(event);

    if (ep == NULL || event == NULL) {
        cleanup(curl, headers);
        return -1;
    }

    wt.readptr = event;
    wt.sizeleft = strlen(event);

    /* In windows, this will init the winsock stuff */
    res = curl_global_init(CURL_GLOBAL_DEFAULT);
    /* Check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "OOB curl_global_init() failed: %s\n",
                curl_easy_strerror(res));
        cleanup(curl, headers);
        return 1;
    }

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl) {
        struct MemoryStruct chunk;

        /* First set the URL that is about to receive our POST. */
        curl_easy_setopt(curl, CURLOPT_URL, ep->enqendpoint);

        /* Now specify we want to POST data */
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        /* we want to use our own read function */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

        /* pointer to pass to our read function */
        curl_easy_setopt(curl, CURLOPT_READDATA, &wt);

        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &chunk);

        /* some servers don't like requests that are made without a user-agent
           field, so we provide one */
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "sfoob/1.0");

        /* complete within 10 seconds */
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 100L);

        /* Set CAbundle */
        getCabundle(caBundle, 512);
        curl_easy_setopt(curl, CURLOPT_CAINFO, caBundle);

        /* Set the expected POST size. If you want to POST large amounts of data,
           consider CURLOPT_POSTFIELDSIZE_LARGE */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) wt.sizeleft);

        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ep->apikey);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            fprintf(stderr, "OOB curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
            cleanup(curl, headers);
            return 2;
        }

    }
    cleanup(curl, headers);
    return 0;
}


