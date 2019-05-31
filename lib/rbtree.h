/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_RBTREE_H
#define SNOWFLAKE_RBTREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "snowflake/platform.h"
#include "snowflake/logger.h"
#include "constants.h"
#include "lib_common.h"

typedef enum color
{
    RED,
    BLACK
}Color;

typedef struct redblack_node
{
    Color color;
    void *elem;
    char *key;
    struct redblack_node *left;
    struct redblack_node *right;
    struct redblack_node *parent;
}RedBlackNode;

typedef RedBlackNode RedBlackTree;

RedBlackTree * STDCALL rbtree_init(void);

SF_INT_RET_CODE STDCALL rbtree_insert(RedBlackTree **T, void *param, char *key);

void * STDCALL rbtree_search_node(RedBlackTree *tree, char *key);

void STDCALL rbtree_deallocate(RedBlackNode *node);

#ifdef __cplusplus
}
#endif

#endif /* SNOWFLAKE_RBTREE_H */
