#ifndef SNOWFLAKE_TEST_SETUP_H
#define SNOWFLAKE_TEST_SETUP_H

#ifdef __cplusplus
extern "C"
{
#endif

// cmocka start
#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
    // cmocka end

#include <time.h>
#include <snowflake/client.h>
#include <snowflake/platform.h>

#define SKIP_IF_PROXY_ENV_IS_SET                                                                                      \
    {                                                                                                                 \
        char envbuf[1024];                                                                                            \
        if (sf_getenv_s("all_proxy", envbuf, sizeof(envbuf)) || sf_getenv_s("https_proxy", envbuf, sizeof(envbuf)) || \
            sf_getenv_s("http_proxy", envbuf, sizeof(envbuf)))                                                        \
        {                                                                                                             \
            return;                                                                                                   \
        }                                                                                                             \
    }

    void initialize_test(sf_bool debug);

    SF_STATUS STDCALL enable_arrow_force(SF_CONNECT *sf);

    SF_CONNECT *setup_snowflake_connection();

    SF_CONNECT *setup_snowflake_connection_with_autocommit(
        const char *timezone, sf_bool autocommit);

    void setup_and_run_query(SF_CONNECT **sfp, SF_STMT **sfstmtp, const char *query);

    void process_results(struct timespec begin, struct timespec end, int num_iterations, const char *label);

    // Setup database with random name for test, return 1 if succeeded, 0 otherwise.
    int setup_random_database();

    void drop_random_database();

    /**
     * Dump error
     * @param error SF_ERROR_STRUCT
     */
    void dump_error(SF_ERROR_STRUCT *error);

#ifdef __cplusplus
}
#endif

#endif // SNOWFLAKE_TEST_SETUP_H
