/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include "paramstore.h"

void STDCALL sf_param_store_init(PARAM_TYPE ptype, void **ps)
{
    PARAM_STORE *pstore = (PARAM_STORE *)SF_CALLOC(1, sizeof(PARAM_STORE));
    switch(ptype)
    {
        case NAMED:
            pstore->param_style = NAMED;
            pstore->tree_map = sf_treemap_init();
            break;
        case POSITIONAL:
            pstore->param_style = POSITIONAL;
            pstore->array_list = sf_array_list_init();
            break;
        default:
            pstore->param_style = INVALID_PARAM_TYPE;
            pstore->array_list = NULL;
            break;
    }
    *ps = (void *)pstore;
}

void STDCALL sf_param_store_deallocate(void *ps)
{
    PARAM_STORE *pstore = (PARAM_STORE *)ps;
    if (pstore->param_style == POSITIONAL)
    {
        sf_array_list_deallocate(pstore->array_list);
    }
    else if (pstore->param_style == NAMED)
    {
        sf_treemap_deallocate(pstore->tree_map);
    }
    SF_FREE(pstore);
}

SF_INT_RET_CODE STDCALL sf_param_store_set(void *ps,
                                void *item,
                                size_t idx,
                                char *name)
{
    PARAM_STORE *pstore = (PARAM_STORE *)ps;
    SF_INT_RET_CODE retval = SF_INT_RET_CODE_SUCCESS;

    if (pstore->param_style == POSITIONAL)
    {
        /*
         * Update sf_array_list_set to return status
         * code;
         */
        sf_array_list_set(pstore->array_list, item, idx);
    }
    else if (pstore->param_style == NAMED)
    {
        retval = sf_treemap_set(pstore->tree_map, item, name);
    }
    return retval;
}
void *STDCALL sf_param_store_get(void *ps, size_t index, char *key)
{
    PARAM_STORE * pstore = (PARAM_STORE *)ps;
    if (pstore->param_style == POSITIONAL)
    {
        if (index < 1)
        {
            log_error("sf_param_store_get: Invalid index for POSITIONAL Params\n");
            return NULL;
        }
        return sf_array_list_get(pstore->array_list,index);
    }
    else if (pstore->param_style == NAMED)
    {
        if (!key)
        {
            log_error("sf_param_store_get: Key NULL for named params \n");
            return NULL;
        }
        return sf_treemap_get(pstore->tree_map, key);
    }
    return NULL;
}