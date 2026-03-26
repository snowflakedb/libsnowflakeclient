#ifndef SNOWFLAKE_MUTEX_H
#define SNOWFLAKE_MUTEX_H

#include "snowflake/client.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif
    /**
    * Create a recursive mutex in C.
    *
    * @param mutex                 Mutex
    * @param id                    Connection address
    */
    sf_bool create_recursive_mutex(void** mutex, uint64 id);

     /**
    * delete a recursive mutex in C.
    *
    * @param mutex                 Mutex
    */
    void free_recursive_mutex(void** mutex);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_MUTEX_H
