/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKEINCLUDE_H
#define SNOWFLAKECLIENT_SNOWFLAKEINCLUDE_H

namespace Snowflake {
    namespace CAPI {
#include <snowflake/client.h>
#undef inline
#undef class
    }
}

//namespace Snowflake {
//    namespace Client {
//        using Snowflake::CAPI::SF_CONNECT;
//        using Snowflake::CAPI::SF_STMT;
//    }
//}

#endif //SNOWFLAKECLIENT_SNOWFLAKEINCLUDE_H
