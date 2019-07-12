/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_MEMORY_H
#define SNOWFLAKE_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <snowflake/basic_types.h>
#include "snowflake/platform.h"

#define SF_MALLOC(s) sf_malloc(s, __FILE__, __LINE__)
#define SF_CALLOC(n, s) sf_calloc(n, s, __FILE__, __LINE__)
#define SF_REALLOC(p, s) sf_realloc(p, s, __FILE__, __LINE__)
#define SF_FREE(p) ((void) (sf_free(p, __FILE__, __LINE__), (p) = NULL))

typedef struct SF_INTERNAL_MEM_HOOKS {
    void *(*alloc)(size_t size);
    void (*dealloc)(void *ptr);
    void *(*realloc)(void *ptr, size_t size);
    void *(*calloc)(size_t nitems, size_t size);
} SF_INTERNAL_MEM_HOOKS;

static SF_INTERNAL_MEM_HOOKS global_hooks = {malloc, free, realloc, calloc};

void sf_memory_init();
void sf_memory_term();
void *sf_malloc(size_t size, const char *file, int line);
void *sf_calloc(size_t num, size_t size, const char *file, int line);
void *sf_realloc(void *ptr, size_t size, const char *file, int line);
void sf_free(void *ptr, const char *file, int line);
void sf_alloc_map_to_log(sf_bool cleanup);

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_MEMORY_H
