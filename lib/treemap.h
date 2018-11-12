/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_TREEMAP_H
#define SNOWFLAKE_TREEMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "snowflake/platform.h"
#include "lib_common.h"
#include "rbtree.h"

#define TREE_MAP_MAX_SIZE 1000
#define HASH_CONSTANT 31

typedef struct sf_treemap_node
{
    RedBlackTree *tree;
} TREE_MAP;

/* 
** Treemap needs to be initialized
** to a size which is sufficiently 
** large and is a prime number
** @return SF_STATUS 
*/
TREE_MAP * STDCALL sf_treemap_init(void);

/* sf_treemap_set
** insert void into treemap
** @return- void
*/

/* TODO - Potentially change this from a void
** to a return status type
*/
SF_RET_CODE STDCALL sf_treemap_set(TREE_MAP *tree_map, void *param, char *key);

/*
** sf_treemap_get
** get params corresponding to a key from the treemap
** @return void struct
*/
void * STDCALL sf_treemap_get(TREE_MAP *tree_map, char *key);

/* sf_treemap_deallocate
** deallocate treemap and any chained lists.
** @return SF_STATUS
*/
void STDCALL sf_treemap_deallocate(TREE_MAP *tree_map);

#ifdef __cplusplus
}
#endif

#endif /* SNOWFLAKE_HASHMAP_H */
