/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_PARAMSTORE_H
#define SNOWFLAKE_PARAMSTORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "memory.h"
#include "treemap.h"
#include "arraylist.h"

typedef enum {
    INVALID_PARAM_TYPE,
    POSITIONAL,
    NAMED
} PARAM_TYPE;

typedef struct param_store
{
    PARAM_TYPE param_style;
    union
    {
        TREE_MAP *tree_map;
        ARRAY_LIST *array_list;
    };
}PARAM_STORE;

void STDCALL sf_param_store_init(PARAM_TYPE ptype,
                                 void **pstore);

void STDCALL sf_param_store_deallocate(void *ps);

SF_INT_RET_CODE STDCALL sf_param_store_set(void *ps,
                                void *item,
                                size_t idx,
                                char *name);

void *STDCALL sf_param_store_get(void *ps,
                                 size_t index,
                                 char *key);
#ifdef __cplusplus
}
#endif

#endif /* SNOWFLAKE_PARAMSTORE_H */