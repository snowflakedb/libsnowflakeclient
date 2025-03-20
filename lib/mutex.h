#ifndef SNOWFLAKE_MUTEX_H
#define SNOWFLAKE_MUTEX_H

#include "snowflake/client.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif
    void create_recursive_mutex(void** mutex, uint64_t id);

    void free_recursive_mutex(void** mutex);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_MUTEX_H
