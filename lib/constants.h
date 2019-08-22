/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_CONSTANTS_H
#define SNOWFLAKE_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <snowflake/basic_types.h>
#include "snowflake/platform.h"

extern sf_bool DISABLE_VERIFY_PEER;
extern char *CA_BUNDLE_FILE;
extern int32 SSL_VERSION;
extern sf_bool DEBUG;
extern sf_bool SF_OCSP_CHECK;
extern char *SF_HEADER_USER_AGENT;

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_CONSTANTS_H
