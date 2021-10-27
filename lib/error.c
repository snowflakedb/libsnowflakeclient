/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include "error.h"
#include "memory.h"

/*
 * Shared message buffer for emergency use.
 */
static SF_MUTEX_HANDLE mutex_shared_msg;
static char _shared_msg[8192];

void STDCALL sf_error_init() {
    _mutex_init(&mutex_shared_msg);
}

void STDCALL sf_error_term() {
    _mutex_term(&mutex_shared_msg);
}

void STDCALL set_snowflake_error(SF_ERROR_STRUCT *error,
                                 SF_STATUS error_code,
                                 const char *msg,
                                 const char *sqlstate,
                                 const char *sfqid,
                                 const char *file,
                                 int line) {
    size_t msglen = strlen(msg);
    // NULL error passed in. Should never happen
    if (!error) {
        return;
    }

    error->error_code = error_code;
    sb_strncpy(error->sfqid, SF_UUID4_LEN, sfqid, SF_UUID4_LEN);
    // Null terminate
    if (error->sfqid[SF_UUID4_LEN - 1] != '\0') {
        error->sfqid[SF_UUID4_LEN - 1] = '\0';
    }

    if (sqlstate != NULL) {
        /* set SQLState if specified */
        sb_strncpy(error->sqlstate, sizeof(error->sqlstate), sqlstate, sizeof(error->sqlstate));
        error->sqlstate[sizeof(error->sqlstate) - 1] = '\0';
    }

    if (error->msg != NULL && !error->is_shared_msg) {
        /* free message buffer first if already exists */
        SF_FREE(error->msg);
    }

    /* allocate new memory */
    error->msg = SF_CALLOC(msglen + 1, sizeof(char));
    if (error->msg != NULL) {
        sb_strncpy(error->msg, msglen + 1, msg, msglen);
        error->msg[msglen] = '\0';
        error->is_shared_msg = SF_BOOLEAN_FALSE;
    } else {
        /* if failed to allocate a memory to store error */
        _mutex_lock(&mutex_shared_msg);
        size_t len =
          msglen > sizeof(_shared_msg) ? sizeof(_shared_msg) : msglen;
        memset(_shared_msg, 0, sizeof(_shared_msg));
        sb_strncpy(_shared_msg, sizeof(_shared_msg), msg, len);
        _shared_msg[sizeof(_shared_msg) - 1] = '\0';
        _mutex_unlock(&mutex_shared_msg);

        error->is_shared_msg = SF_BOOLEAN_TRUE;
        error->msg = _shared_msg;
    }

    error->file = (char *) file;
    error->line = line;
}

void STDCALL clear_snowflake_error(SF_ERROR_STRUCT *error) {
    _mutex_lock(&mutex_shared_msg);
    memset(_shared_msg, 0, sizeof(_shared_msg));
    _mutex_unlock(&mutex_shared_msg);
    if (strncmp(error->sqlstate, SF_SQLSTATE_NO_ERROR,
                sizeof(SF_SQLSTATE_NO_ERROR)) && !error->is_shared_msg) {
        /* Error already set and msg is not on shared mem */
        SF_FREE(error->msg);
    }
    sb_strcpy(error->sqlstate, sizeof(error->sqlstate), SF_SQLSTATE_NO_ERROR);
    error->error_code = SF_STATUS_SUCCESS;
    error->msg = NULL;
    error->file = NULL;
    error->line = 0;
    error->is_shared_msg = SF_BOOLEAN_FALSE;
    memset(error->sfqid, 0, SF_UUID4_LEN);
}

void STDCALL copy_snowflake_error(SF_ERROR_STRUCT *dst, SF_ERROR_STRUCT *src) {
    if (!dst || !src) {
        return;
    }

    set_snowflake_error(dst, src->error_code, src->msg, src->sqlstate,
                        src->sfqid, src->file, src->line);
}
