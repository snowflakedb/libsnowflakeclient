/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_CHUNK_DOWNLOADER_H
#define SNOWFLAKE_CHUNK_DOWNLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")

#define CURL_STATICLIB 
#include <curl/curl.h>
#include <snowflake/client.h>
#include "snowflake/platform.h"
#include "cJSON.h"
#include "connection.h"

typedef struct SF_QUEUE_ITEM {
    char *url;
    int64 row_count;
    cJSON *chunk;
} SF_QUEUE_ITEM;

struct SF_CHUNK_DOWNLOADER {
    uint64 thread_count;

    // Threads
    SF_THREAD_HANDLE *threads;

    // Queue
    SF_CRITICAL_SECTION_HANDLE queue_lock;
    SF_CONDITION_HANDLE producer_cond;
    SF_CONDITION_HANDLE consumer_cond;

    // A "queue" that is actually just a locked array
    SF_QUEUE_ITEM* queue;

    // Queue attributes
    uint64 producer_head;
    uint64 consumer_head;
    uint64 queue_size;

    // Chunk downloader connection attributes
    char *qrmk;
    SF_HEADER *chunk_headers;

    // Error/shutdown flags
    sf_bool is_shutdown;
    sf_bool has_error;

    // Chunk downloader attribute read-write lock. If you need to acquire both the queue_lock and attr_lock,
    // ALWAYS acquire the queue_lock first, otherwise we can deadlock
    SF_RWLOCK_HANDLE attr_lock;

    // Snowflake statement error
    SF_ERROR_STRUCT *sf_error;

    // Snowflake connection insecure mode flag
    sf_bool insecure_mode;
};

SF_CHUNK_DOWNLOADER *STDCALL chunk_downloader_init(const char *qrmk,
                                                   cJSON* chunk_headers,
                                                   cJSON *chunks,
                                                   uint64 thread_count,
                                                   uint64 fetch_slots,
                                                   SF_ERROR_STRUCT *sf_error,
                                                   sf_bool insecure_mode);
sf_bool STDCALL chunk_downloader_term(SF_CHUNK_DOWNLOADER *chunk_downloader);
sf_bool STDCALL get_shutdown_or_error(SF_CHUNK_DOWNLOADER *chunk_downloader);
sf_bool STDCALL get_shutdown(SF_CHUNK_DOWNLOADER *chunk_downloader);
sf_bool STDCALL get_error(SF_CHUNK_DOWNLOADER *chunk_downloader);

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_CHUNK_DOWNLOADER_H
