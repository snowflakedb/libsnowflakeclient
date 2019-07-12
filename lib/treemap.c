/*
** Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
*/

#include <string.h>
#include "treemap.h"
#include "memory.h"


TREE_MAP * STDCALL sf_treemap_init()
{
    TREE_MAP *tree_map = (TREE_MAP *)SF_CALLOC(TREE_MAP_MAX_SIZE, sizeof(TREE_MAP));
    if (!tree_map)
    {
        log_error("sf_treemap_init: Memory Allocation failed\n");
    }
    
    return tree_map;
}

/*
** The hash fxn used by the treemap
** should follow horner's rule 
** to generate index
** @return an index in the treemap
*/
unsigned long STDCALL sf_treemap_hash_fxn(char *key)
{
    unsigned int iter = 0;
    unsigned long hash = 0;
    size_t len = 0;

    len = strlen(key);

    for (iter = 0; iter < len; iter++)
    {
        hash = (HASH_CONSTANT*hash)+key[iter];
    }
    return hash;
}

/* 
** Retrieves an index from the tree map
** to store a bind param at.
** @return - unsigned long
*/
unsigned long STDCALL sf_treemap_get_index(char *key)
{
    unsigned long hash = sf_treemap_hash_fxn(key);

    hash = hash % TREE_MAP_MAX_SIZE;
    return hash;
}

/*
** Inserts the bind params at the right
** index in the tree map. This also takes
** care of collisions
** @return - SF_INT_RET_CODE
*/

SF_INT_RET_CODE STDCALL sf_treemap_insert_node(unsigned long index, TREE_MAP *tree_map, void *param, char *key)
{
    TREE_MAP *idx_cur;

    idx_cur = &tree_map[index];

    if (!idx_cur->tree)
    {
        idx_cur->tree = rbtree_init();
    }

    return rbtree_insert(&idx_cur->tree, param, key);
}

/* sf_treemap_extract_node
** extract a node corresponding to a key
** from the treemap
** @return - SF_INT_RET_CODE based on whether node
** was successfully extracted.
*/

SF_INT_RET_CODE STDCALL sf_treemap_extract_node(TREE_MAP *tree_map, int idx, char *key, void **ret_param)
{
    TREE_MAP *cur_node;
    SF_INT_RET_CODE retval = SF_INT_RET_CODE_ERROR;
    if (tree_map && (cur_node = &tree_map[idx]))
    {
        *ret_param = rbtree_search_node(cur_node->tree, key);
        if (!*ret_param)
        {
            log_debug("sf_treemap_extract_node: Node not found");
            retval = SF_INT_RET_CODE_NODE_NOT_FOUND;
            goto done;
        }
    }
    else
    {
        if (!tree_map)
        {
            log_debug("sf_treemap_extract_node: tree_map passed is NULL");
            goto done;
        }
        else if (!cur_node)
        {
            log_debug("sf_treemap_extract_node: cur_node is NULL\n");
            goto done;
        }
    }
    retval = SF_INT_RET_CODE_SUCCESS;
done:

    return retval;
}

SF_INT_RET_CODE STDCALL sf_treemap_set(TREE_MAP *tree_map, void *param, char *key)
{

    if (!tree_map || !param || !key)
    {
        /* Handle error 
        ** return or goto done;
        */
        log_error("sf_treemap_set: Tree Map || Param || key passed is NULL\n");
        return SF_INT_RET_CODE_ERROR;
    }

    return sf_treemap_insert_node(sf_treemap_get_index(key), tree_map, param, key);
}

void * STDCALL sf_treemap_get(TREE_MAP *tree_map, char *key)
{
    void *param = NULL;
    if (!tree_map || !key)
    {
        return NULL;
    }

    sf_treemap_extract_node(tree_map, sf_treemap_get_index(key), key, &param);
    if (!param)
    {
        log_debug("sf_treemap_get: param returned is NULL\n");
    }
    return param;
}

void STDCALL sf_treemap_deallocate(TREE_MAP *tree_map)
{
    int iter = 0;

    if (!tree_map)
    {
        log_debug("sf_treemap_deallocate: treemap is NULL\n");
        return;
    }
    for (iter = 0; iter < TREE_MAP_MAX_SIZE; iter++)
    {
        if (tree_map[iter].tree)
        {
            rbtree_deallocate(tree_map[iter].tree);
        }
        tree_map[iter].tree = NULL;
    }
    SF_FREE(tree_map);
}
