#ifndef SNOWFLAKE_QUERY_CONTEXT_CACHE_H
#define SNOWFLAKE_QUERY_CONTEXT_CACHE_H

#include "snowflake/client.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Initialize query context cache
     *
     * @param conn                 The connection
     *
     */
    void qcc_initialize(SF_CONNECT * conn);

    /**
    * set query context cache capacity
    *
    * @param capacity              The max size of query context cache
    *
    */
    void qcc_set_capacity(SF_CONNECT * conn, uint64 capacity);

    /**
     * Retrieve serialized query context cache for sending in query request
     *
     * @param conn                 The connection
     *
     * @return serialized query context cache in JSON
     */
    cJSON* qcc_serialize(SF_CONNECT * conn);

    /**
     * Deserialize query context from query response and merge into cache
     *
     * @param conn                 The connection
     * @param query_context        The JSON node of query context from query response
     *
     */
    void qcc_deserialize(SF_CONNECT * conn, cJSON* query_context);

    /**
    * Terminate query context cache
    *
    * @param conn                 The connection
    */
    void qcc_terminate(SF_CONNECT * conn);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_QUERY_CONTEXT_CACHE_H
