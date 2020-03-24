#ifndef __OOBTELEMETRY_H__
#define __OOBTELEMETRY_H__

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include "jsonutil.h"

//event will be Freed by the callee
//We will modify event to mask passwords/secrets
extern int sendOOBevent(char *event);

//connStr will be copied into another string
extern void setConnectionString(char const* connStr);

extern char *prepareOOBevent(oobOcspData *ocspevent);

extern void setOOBeventdata(enum OOBINFO id, const char *data, long num);

extern void getCabundle(char *cabundle, int maxlen);

extern void setoobConnectioninfo(const char* host,
    const char* port,
    const char* account,
    const char* user,
    const char* token,
    const char* authenticator,
    const char* dbName,
    const char* schema,
    const char* warehouse,
    const char* role,
    short ssl
    );

#endif
