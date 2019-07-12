/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_LIB_COMMON_H
#define SNOWFLAKE_LIB_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Internal error codes.
 */
typedef enum SF_INT_RET_CODE
{
    /* COMMON CODES */
    SF_INT_RET_CODE_SUCCESS,
    SF_INT_RET_CODE_ERROR,

    /* TREEMAP SPECIFIC CODES */
    SF_INT_RET_CODE_BAD_INDEX,

    /* RBTREE SPECIFIC CODES */
    SF_INT_RET_CODE_NODE_NOT_FOUND,
    SF_INT_RET_CODE_DUPLICATES
}SF_INT_RET_CODE;

#ifdef __cplusplus
}
#endif

#endif