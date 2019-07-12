/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_ERROR_H
#define SNOWFLAKE_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <snowflake/client.h>
#include "snowflake/platform.h"

#define SET_SNOWFLAKE_ERROR(e, ec, m, sqlstate) set_snowflake_error(e, ec, m, sqlstate, "", __FILE__, __LINE__)
#define SET_SNOWFLAKE_STMT_ERROR(e, ec, m, sqlstate, uuid) set_snowflake_error(e, ec, m, sqlstate, uuid, __FILE__, __LINE__)

void STDCALL sf_error_init();
void STDCALL sf_error_term();
void STDCALL set_snowflake_error(SF_ERROR_STRUCT *error,
                                 SF_STATUS error_code,
                                 const char *msg,
                                 const char *sqlstate,
                                 const char *sfqid,
                                 const char *file,
                                 int line);

void STDCALL clear_snowflake_error(SF_ERROR_STRUCT *error);

void STDCALL copy_snowflake_error(SF_ERROR_STRUCT *dst, SF_ERROR_STRUCT *src);

#define ERR_MSG_ACCOUNT_PARAMETER_IS_MISSING "account parameter is missing"
#define ERR_MSG_USER_PARAMETER_IS_MISSING "user parameter is missing"
#define ERR_MSG_PASSWORD_PARAMETER_IS_MISSING "password parameter is missing"
#define ERR_MSG_CONNECTION_ALREADY_EXISTS "Connection already exists."
#define ERR_MSG_SESSION_TOKEN_INVALID "The session token is invalid. Please reconnect"
#define ERR_MSG_GONE_SESSION "The session no longer exists on the server. Please reconnect"

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_ERROR_H
