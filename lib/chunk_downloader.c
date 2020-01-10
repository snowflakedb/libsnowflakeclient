/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <errno.h>
#include <string.h>
#include "chunk_downloader.h"
#include "memory.h"
#include "connection.h"
#include "error.h"
#include "client_int.h"

static void* chunk_downloader_thread(void *downloader);
static void STDCALL set_shutdown(SF_CHUNK_DOWNLOADER *chunk_downloader, sf_bool value);
static void STDCALL set_error(SF_CHUNK_DOWNLOADER *chunk_downloader, sf_bool value);

#define PTHREAD_LOCK_INIT_ERROR_MSG(e, em) \
switch(e) \
{ \
    case EAGAIN: (em) = "System lacked resources to create lock"; break; \
    case ENOMEM: (em) = "Insufficient memory to create mutex"; break; \
    case EPERM: (em) = "Caller doesn't have the privilege to perform the operation"; break; \
    case EBUSY: (em) = "Mutex already initialized"; break; \
    case EINVAL: (em) = "The value specified by attr is invalid"; break; \
    default: (em) = "Unknown non-zero pthread init error" ; break; \
}

#define PTHREAD_CREATE_ERROR_MSG(e, em) \
switch(e) \
{ \
    case EAGAIN: (em) = "System lacked resources to create another thread"; break; \
    case EPERM: (em) = "Caller doesn't have the privilege to set the required scheduling parameters"; break; \
    case EINVAL: (em) = "The value specified by attr is invalid"; break; \
    default: (em) = "Unknown non-zero pthread init error" ; break; \
}

#define PTHREAD_JOIN_ERROR_MSG(e, em) \
switch(e) \
{ \
    case EDEADLK: (em) = "A deadlock was detected (i.e. two threads tried to join with each other)"; break; \
    case EPERM: (em) = "Not a joinable thread"; break; \
    case ESRCH: (em) = "No thread with specified ID could be found"; break; \
    default: (em) = "Unknown non-zero pthread join error" ; break; \
}

sf_bool STDCALL get_shutdown_or_error(struct SF_CHUNK_DOWNLOADER *chunk_downloader) {
    sf_bool ret;
    _rwlock_rdlock(&chunk_downloader->attr_lock);
    ret = chunk_downloader->is_shutdown || chunk_downloader->has_error;
    _rwlock_rdunlock(&chunk_downloader->attr_lock);
    return ret;
}

sf_bool STDCALL get_shutdown(struct SF_CHUNK_DOWNLOADER *chunk_downloader) {
    sf_bool ret;
    _rwlock_rdlock(&chunk_downloader->attr_lock);
    ret = chunk_downloader->is_shutdown;
    _rwlock_rdunlock(&chunk_downloader->attr_lock);
    return ret;
}

static void STDCALL set_shutdown(struct SF_CHUNK_DOWNLOADER *chunk_downloader, sf_bool value) {
    _rwlock_wrlock(&chunk_downloader->attr_lock);
    chunk_downloader->is_shutdown = value;
    _rwlock_wrunlock(&chunk_downloader->attr_lock);
}

sf_bool STDCALL get_error(struct SF_CHUNK_DOWNLOADER *chunk_downloader) {
    sf_bool ret;
    _rwlock_rdlock(&chunk_downloader->attr_lock);
    ret = chunk_downloader->has_error;
    _rwlock_rdunlock(&chunk_downloader->attr_lock);
    return ret;
}

static void STDCALL set_error(struct SF_CHUNK_DOWNLOADER *chunk_downloader, sf_bool value) {
    _rwlock_wrlock(&chunk_downloader->attr_lock);
    chunk_downloader->has_error = value;
    _rwlock_wrunlock(&chunk_downloader->attr_lock);
}

sf_bool STDCALL init_locks(struct SF_CHUNK_DOWNLOADER *chunk_downloader) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    SF_ERROR_STRUCT *error = chunk_downloader->sf_error;
    int pthread_ret;
    const char *error_msg;

    if ((pthread_ret = _critical_section_init(&chunk_downloader->queue_lock)) != 0) {
        PTHREAD_LOCK_INIT_ERROR_MSG(pthread_ret, error_msg);
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_PTHREAD, error_msg, "");
        goto cleanup;
    }

    if ((pthread_ret = _rwlock_init(&chunk_downloader->attr_lock)) != 0) {
        PTHREAD_LOCK_INIT_ERROR_MSG(pthread_ret, error_msg);
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_PTHREAD, error_msg, "");
        goto cleanup;
    }
    if ((pthread_ret = _cond_init(&chunk_downloader->producer_cond)) != 0) {
        PTHREAD_LOCK_INIT_ERROR_MSG(pthread_ret, error_msg);
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_PTHREAD, error_msg, "");
        goto cleanup;
    }
    if ((pthread_ret = _cond_init(&chunk_downloader->consumer_cond)) != 0) {
        PTHREAD_LOCK_INIT_ERROR_MSG(pthread_ret, error_msg);
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_PTHREAD, error_msg, "");
        goto cleanup;
    }
    // Success
    ret = SF_BOOLEAN_TRUE;

    return ret;

cleanup:
    // We may destroy some uninitialized locks/conds, but we don't care.
    _critical_section_term(&chunk_downloader->queue_lock);
    _cond_term(&chunk_downloader->producer_cond);
    _cond_term(&chunk_downloader->consumer_cond);
    _rwlock_term(&chunk_downloader->attr_lock);
    return ret;
}

sf_bool STDCALL fill_queue(struct SF_CHUNK_DOWNLOADER *chunk_downloader, cJSON *chunks, int chunk_count) {
    int i;
    cJSON *chunk = NULL;

    // We want to detach each chunk object so that after we create the queue item,
    // we free the memory associated with the JSON blob
    for (i = 0; i < chunk_count; i++) {
        // Detach instead of getting so that we can free the chunk memory after we create our queue item
        if (json_detach_object_from_array(&chunk, chunks, 0)) {
            goto cleanup;
        }

        chunk_downloader->queue[i].url = NULL;
        chunk_downloader->queue[i].row_count = 0;
        chunk_downloader->queue[i].chunk = NULL;

        if (json_copy_string(&chunk_downloader->queue[i].url, chunk, "url")) {
            goto cleanup;
        }

        // We need to update queue_size here to reflect the fact that we've already allocated memory for URL.
        // This is because we use queue_size to free the created URLs if we run into an error so we must increase size
        // here incase if we successfully copy the URL from the chunk, but fail to copy rowCount from the chunk
        chunk_downloader->queue_size++;

        if (json_copy_int(&chunk_downloader->queue[i].row_count, chunk, "rowCount")) {
            goto cleanup;
        }

        // Free detached chunk
      snowflake_cJSON_Delete(chunk);
        chunk = NULL;
    }

    return SF_BOOLEAN_TRUE;

cleanup:
    for (i = 0; i < chunk_downloader->queue_size; i++) {
        SF_FREE(chunk_downloader->queue[i].url);
    }

    return SF_BOOLEAN_FALSE;
}

sf_bool STDCALL create_chunk_headers(struct SF_CHUNK_DOWNLOADER *chunk_downloader, cJSON *json_headers) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    size_t header_field_size;
    size_t i;
    cJSON *item = NULL;
    char *header_item = NULL;
    ARRAY_LIST *keys = json_get_object_keys(json_headers);
    char *key;

    for (i = 0; i < keys->used; i++) {
        key = (char *) sf_array_list_get(keys, i);
        // Since I know that these keys are correct from a case sensitive view,
        // I can use the faster case sensitive version
        item = snowflake_cJSON_GetObjectItemCaseSensitive(json_headers, key);
        if (!item || !key || !item->valuestring) {
            SET_SNOWFLAKE_ERROR(chunk_downloader->sf_error, SF_STATUS_ERROR_BAD_JSON,
                                "Could not find critical chunk header item", "");
            goto cleanup;
        }
        header_field_size = strlen(key) + strlen(item->valuestring) + 2;

        // Type conversion is safe since we know that header_field_size must be positive
        header_item = (char *) SF_CALLOC(1, header_field_size + 1);
        sb_sprintf(header_item, header_field_size + 1, "%s: %s", key, item->valuestring);
        chunk_downloader->chunk_headers->header = curl_slist_append(chunk_downloader->chunk_headers->header, header_item);
        SF_FREE(header_item);
    }

    ret = SF_BOOLEAN_TRUE;

cleanup:
    sf_array_list_deallocate(keys);
    SF_FREE(header_item);

    return ret;
}

sf_bool STDCALL download_chunk(char *url, SF_HEADER *headers, cJSON **chunk, SF_ERROR_STRUCT *error, sf_bool insecure_mode) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    CURL *curl = NULL;
    curl = curl_easy_init();

    if (!curl || !http_perform(curl, GET_REQUEST_TYPE, url, headers, NULL, chunk, DEFAULT_SNOWFLAKE_REQUEST_TIMEOUT, SF_BOOLEAN_TRUE, error, insecure_mode)) {
        // Error set in perform function
        goto cleanup;
    }

    ret = SF_BOOLEAN_TRUE;

cleanup:
    curl_easy_cleanup(curl);

    return ret;
}

SF_CHUNK_DOWNLOADER *STDCALL chunk_downloader_init(const char *qrmk,
                                                   cJSON *chunk_headers,
                                                   cJSON *chunks,
                                                   uint64 thread_count,
                                                   uint64 fetch_slots,
                                                   SF_ERROR_STRUCT *sf_error,
                                                   sf_bool insecure_mode) {
    struct SF_CHUNK_DOWNLOADER *chunk_downloader = NULL;
    const char *error_msg = NULL;
    int chunk_count;
    int i;
    int pthread_ret;
    size_t qrmk_len = 1;
    // We need thread_count, fetch_slots, chunks, and either qrmk or chunk_headers
    if (thread_count <= 0 ||
            fetch_slots <= 0 ||
            !chunks ||
            !snowflake_cJSON_IsArray(chunks) ||
            strcmp(chunks->string, "chunks") != 0) {
        return NULL;
    }

    if ((chunk_downloader = (SF_CHUNK_DOWNLOADER *) SF_CALLOC(1, sizeof(SF_CHUNK_DOWNLOADER))) == NULL) {
        return NULL;
    }

    // Initialize default values
    chunk_downloader->threads = NULL;
    chunk_downloader->queue = NULL;
    chunk_downloader->qrmk = NULL;
    chunk_downloader->chunk_headers = sf_header_create();
    chunk_downloader->thread_count = 0;
    chunk_downloader->queue_size = 0;
    chunk_downloader->producer_head = 0;
    chunk_downloader->consumer_head = 0;
    chunk_downloader->is_shutdown = SF_BOOLEAN_FALSE;
    chunk_downloader->has_error = SF_BOOLEAN_FALSE;
    chunk_downloader->sf_error = sf_error;
    chunk_downloader->insecure_mode = insecure_mode;

    // Initialize chunk_headers or qrmk
    if (chunk_headers) {
        if(!create_chunk_headers(chunk_downloader, chunk_headers)) {
            goto cleanup;
        }
    } else if (qrmk) {
        qrmk_len += strlen(qrmk);
        chunk_downloader->qrmk = (char *) SF_CALLOC(1, qrmk_len);
        sb_strncpy(chunk_downloader->qrmk, qrmk_len, qrmk, qrmk_len);
    }

    // Initialize mutexes and conditional variables
    if (!init_locks(chunk_downloader)) {
        goto cleanup;
    }

    // Initialize queue and thread memory
    chunk_count = snowflake_cJSON_GetArraySize(chunks);
    chunk_downloader->threads = (SF_THREAD_HANDLE *)SF_CALLOC((int)thread_count, sizeof(SF_THREAD_HANDLE));
    chunk_downloader->queue = (SF_QUEUE_ITEM *) SF_CALLOC(chunk_count, sizeof(SF_QUEUE_ITEM));
    if (!chunk_downloader->threads || !chunk_downloader->queue) {
        goto cleanup;
    }

    // Fill up queue
    if (!fill_queue(chunk_downloader, chunks, chunk_count)) {
        goto cleanup;
    }

    // Initialize threads
    for (i = 0; i < thread_count; i++) {
        // If non-zero exit code, terminate chunk downloader
        if ((pthread_ret = _thread_init(
              &chunk_downloader->threads[i],
              chunk_downloader_thread,
              (void *)chunk_downloader)) != 0) {
            chunk_downloader_term(chunk_downloader);
            PTHREAD_CREATE_ERROR_MSG(pthread_ret, error_msg);
            SET_SNOWFLAKE_ERROR(sf_error, SF_STATUS_ERROR_PTHREAD, error_msg, "");
            return NULL;
        }

        chunk_downloader->thread_count++;
    }

    return chunk_downloader;

cleanup:
    if (chunk_downloader) {
        SF_FREE(chunk_downloader->qrmk);
        sf_header_destroy(chunk_downloader->chunk_headers);
        SF_FREE(chunk_downloader->queue);
        SF_FREE(chunk_downloader->threads);
    }
    SF_FREE(chunk_downloader);

    return NULL;
}


sf_bool STDCALL chunk_downloader_term(struct SF_CHUNK_DOWNLOADER *chunk_downloader) {
    int pthread_ret;
    const char *error_msg;
    uint64 i;
    if (!chunk_downloader) {
        return SF_BOOLEAN_FALSE;
    }

    if ((pthread_ret = _critical_section_lock(&chunk_downloader->queue_lock))) {
        _rwlock_wrlock(&chunk_downloader->attr_lock);
        if (!chunk_downloader->has_error) {
            PTHREAD_LOCK_INIT_ERROR_MSG(pthread_ret, error_msg);
            SET_SNOWFLAKE_ERROR(chunk_downloader->sf_error, SF_STATUS_ERROR_PTHREAD, error_msg, "");
            chunk_downloader->has_error = SF_BOOLEAN_TRUE;
        }
        _rwlock_wrunlock(&chunk_downloader->attr_lock);
        return SF_BOOLEAN_FALSE;
    }

    do {
        // Already shutting down, just return false
        if (get_shutdown(chunk_downloader)) {
            return SF_BOOLEAN_FALSE;
        }

        set_shutdown(chunk_downloader, SF_BOOLEAN_TRUE);

        if (_cond_broadcast(&chunk_downloader->consumer_cond) ||
            _cond_broadcast(&chunk_downloader->producer_cond) ||
                (_critical_section_unlock(&chunk_downloader->queue_lock))) {
            // Something went wrong with either notifying the producer/consumer or releasing the queue lock
            // Set and error and then try to continue with cleanup
            _rwlock_wrlock(&chunk_downloader->attr_lock);
            if (!chunk_downloader->has_error) {
                SET_SNOWFLAKE_ERROR(chunk_downloader->sf_error, SF_STATUS_ERROR_PTHREAD, "Error during condition broadcast", "");
                chunk_downloader->has_error = SF_BOOLEAN_TRUE;
            }
            _rwlock_wrunlock(&chunk_downloader->attr_lock);
        }

        // Join all the threads
        for (i = 0; i < chunk_downloader->thread_count; i++) {
            if ((pthread_ret = _thread_join(chunk_downloader->threads[i])) != 0) {
                if (!get_error(chunk_downloader)) {
                    PTHREAD_JOIN_ERROR_MSG(pthread_ret, error_msg);
                    SET_SNOWFLAKE_ERROR(chunk_downloader->sf_error, SF_STATUS_ERROR_PTHREAD, error_msg, "");
                }
                _rwlock_wrlock(&chunk_downloader->attr_lock);
                if (!chunk_downloader->has_error) {
                    PTHREAD_JOIN_ERROR_MSG(pthread_ret, error_msg);
                    SET_SNOWFLAKE_ERROR(chunk_downloader->sf_error, SF_STATUS_ERROR_PTHREAD, error_msg, "");
                    chunk_downloader->has_error = SF_BOOLEAN_TRUE;
                }
                _rwlock_wrunlock(&chunk_downloader->attr_lock);
            }
        }
    } while (0);

    // Free chunk downloader memory
    SF_FREE(chunk_downloader->threads);
    // Free all the memory of the items in the queue before freeing queue memory
    for (i = 0; i < chunk_downloader->queue_size; i++) {
        SF_FREE(chunk_downloader->queue[i].url);
      snowflake_cJSON_Delete(chunk_downloader->queue[i].chunk);
    }
    SF_FREE(chunk_downloader->queue);
    SF_FREE(chunk_downloader->qrmk);
    sf_header_destroy(chunk_downloader->chunk_headers);
    _critical_section_term(&chunk_downloader->queue_lock);
    _cond_term(&chunk_downloader->producer_cond);
    _cond_term(&chunk_downloader->consumer_cond);
    _rwlock_term(&chunk_downloader->attr_lock);
    SF_FREE(chunk_downloader);

    return SF_BOOLEAN_TRUE;
}

static void * chunk_downloader_thread(void *downloader) {
    struct SF_CHUNK_DOWNLOADER *chunk_downloader = (SF_CHUNK_DOWNLOADER *) downloader;
    cJSON *chunk = NULL;
    uint64 index;
    // Create err per thread so we don't have to lock the chunk downloader err
    SF_ERROR_STRUCT err;
    memset(&err, 0, sizeof(err));
    clear_snowflake_error(&err);

    // Loop forever until shutdown
    while (1) {
        // Reset from previous loop
        chunk = NULL;
        _critical_section_lock(&chunk_downloader->queue_lock);

        // If we've downloaded chunks == # of threads, wait until the consumer consumes a chunk.
        // Ensure that the producer_head is less than the queue_size to ensure that we still have items to process
        // If we're shutting down or an err has occurred, skip
        while ((chunk_downloader->producer_head - chunk_downloader->consumer_head) >= chunk_downloader->thread_count &&
                chunk_downloader->producer_head < chunk_downloader->queue_size &&
                !get_shutdown_or_error(chunk_downloader)) {
            _cond_wait(&chunk_downloader->producer_cond, &chunk_downloader->queue_lock);
        }

        // If we're shutting down, or we have reached the end of the results, then break
        if (get_shutdown_or_error(chunk_downloader) || chunk_downloader->producer_head >= chunk_downloader->queue_size) {
            break;
        }

        // Get queue item and set it locally
        index = chunk_downloader->producer_head++;

        // Unlock since we have our queue item, and don't need the lock while we're processing the queue
        _critical_section_unlock(&chunk_downloader->queue_lock);

        // Download chunk
        if (!download_chunk(chunk_downloader->queue[index].url, chunk_downloader->chunk_headers,
                            &chunk, &err, chunk_downloader->insecure_mode)) {
            _rwlock_wrlock(&chunk_downloader->attr_lock);
            if (!chunk_downloader->has_error) {
                copy_snowflake_error(chunk_downloader->sf_error, &err);
                chunk_downloader->has_error = SF_BOOLEAN_TRUE;
            }
            _rwlock_wrunlock(&chunk_downloader->attr_lock);
            break;
        }

        // Gain back lock to set cJSON blob
        _critical_section_lock(&chunk_downloader->queue_lock);

        if (get_error(chunk_downloader)) {
            break;
        }

        // Set the chunk
        chunk_downloader->queue[index].chunk = chunk;

        // Notify the consumer that we have a chunk ready
        if (_cond_signal(&chunk_downloader->consumer_cond)) {
            _rwlock_wrlock(&chunk_downloader->attr_lock);
            if (!chunk_downloader->has_error) {
                SET_SNOWFLAKE_ERROR(chunk_downloader->sf_error, SF_STATUS_ERROR_PTHREAD,
                                    "Error sending consumer signal to notify of chunk downloaded", "");
                chunk_downloader->has_error = SF_BOOLEAN_TRUE;
            }
            _rwlock_wrunlock(&chunk_downloader->attr_lock);
            break;
        }

        // Drop the lock
        _critical_section_unlock(&chunk_downloader->queue_lock);
    }

    _critical_section_unlock(&chunk_downloader->queue_lock);
    _thread_exit();
    return NULL;
}
