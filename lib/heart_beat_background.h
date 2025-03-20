/*
 * Copyright (c) 2022 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_HEARTBEAT_BACKGROUND_H
#define SNOWFLAKE_HEARTBEAT_BACKGROUND_H

#include "snowflake/client.h"

#ifdef __cplusplus
extern "C" {
#endif

    void start_heart_beat_for_this_session(SF_CONNECT* sf);

    void stop_heart_beat_for_this_session(SF_CONNECT* sf);

    void renew_session_sync(SF_CONNECT* sf);
#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_HEARTBEAT_BACKGROUND_H
