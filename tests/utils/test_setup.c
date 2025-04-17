#include <snowflake/logger.h>
#include "snowflake/client.h"
#include <string.h>
#include "test_setup.h"
#include <stdlib.h>
#include <time.h>

extern int uuid4_generate(char *dst);

// Long path space
char PERFORMANCE_TEST_RESULTS_PATH[5000];

// random database namespace
char RANDOM_DATABASE_NAME[100];

void initialize_test(sf_bool debug)
{
    // default location and the maximum logging
    snowflake_global_init(NULL, SF_LOG_TRACE, NULL);

    snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE"));
    snowflake_global_set_attribute(SF_GLOBAL_DEBUG, &debug);

    // Setup performance test log file
    const char *perf_test_fn = "performance_tests.csv";
    const char *test_log_dir = getenv("SNOWFLAKE_TEST_LOG_DIR");
#ifdef _WIN32
    const char *sep = "\\";
#else
    const char *sep = "/";
#endif
    if (!test_log_dir)
    {
        char tmp_dir[4096];
        sf_get_tmp_dir(tmp_dir);
        snprintf(PERFORMANCE_TEST_RESULTS_PATH, 5000, "%s%s%s", tmp_dir, sep, perf_test_fn);
    }
    else
    {
        snprintf(PERFORMANCE_TEST_RESULTS_PATH, 5000, "%s%s%s", test_log_dir, sep, perf_test_fn);
    }
}

SF_CONNECT *setup_snowflake_connection()
{
    return setup_snowflake_connection_with_autocommit(
        "UTC", SF_BOOLEAN_TRUE);
}

/**
 * Enables C_API_QUERY_RESULT_FORMAT='ARROW_FORCE'.
 * This can be removed when Arrow support is officially added.
 */
SF_STATUS STDCALL enable_arrow_force(SF_CONNECT *sf)
{
    /* query */
    SF_STMT *sfstmt = snowflake_stmt(sf);

    SF_STATUS status = snowflake_query(
        sfstmt, "alter session set c_api_query_result_format = 'ARROW_FORCE'",
        0);

    snowflake_stmt_term(sfstmt);

    return status;
}

SF_CONNECT *setup_snowflake_connection_with_autocommit(
    const char *timezone, sf_bool autocommit)
{
    SF_CONNECT *sf = snowflake_init();

    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
                            getenv("SNOWFLAKE_TEST_PASSWORD"));
    snowflake_set_attribute(sf, SF_CON_DATABASE,
                            getenv("SNOWFLAKE_TEST_DATABASE"));
    snowflake_set_attribute(sf, SF_CON_SCHEMA, getenv("SNOWFLAKE_TEST_SCHEMA"));
    snowflake_set_attribute(sf, SF_CON_ROLE, getenv("SNOWFLAKE_TEST_ROLE"));
    snowflake_set_attribute(sf, SF_CON_WAREHOUSE,
                            getenv("SNOWFLAKE_TEST_WAREHOUSE"));
    snowflake_set_attribute(sf, SF_CON_AUTOCOMMIT, &autocommit);
    snowflake_set_attribute(sf, SF_CON_TIMEZONE, timezone);
    char *host, *port, *protocol;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host)
    {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port)
    {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol)
    {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }
    return sf;
}

void dump_error(SF_ERROR_STRUCT *error)
{
    fprintf(stderr, "Error code: %d, message: %s\nIn File, %s, Line, %d\n",
            error->error_code,
            error->msg,
            error->file,
            error->line);
}

void setup_and_run_query(SF_CONNECT **sfp, SF_STMT **sfstmtp, const char *query)
{
    SF_STATUS status;
    SF_CONNECT *sf;
    SF_STMT *sfstmt;
    sf = setup_snowflake_connection();
    // Save created connection
    *sfp = sf;
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    sfstmt = snowflake_stmt(sf);
    // Save created statement
    *sfstmtp = sfstmt;
    status = snowflake_query(
        sfstmt,
        query,
        0);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
}
void process_results(struct timespec begin, struct timespec end, int num_iterations, const char *label)
{
    FILE *results_file = fopen(PERFORMANCE_TEST_RESULTS_PATH, "a+");
    double time_elapsed = (double)(end.tv_sec - begin.tv_sec) + (double)(end.tv_nsec - begin.tv_nsec) / 1000000000;
    fprintf(results_file, "%s, %lf, %i\n", label, time_elapsed, num_iterations);
    // printf("%s, %lf, %i\n", label, time_elapsed, num_iterations);
    fclose(results_file);
}

int setup_random_database()
{
    SF_STATUS status;
    SF_CONNECT *sf;
    SF_STMT *sfstmt;
    char random_db_name[100];
    char uuid[SF_UUID4_LEN];
    char query[200];

    if (strlen(RANDOM_DATABASE_NAME) > 0)
    {
        // already setup
        return 1;
    }

    uuid4_generate(uuid);
    for (int i = 0; i < sizeof(uuid); i++)
    {
        if (uuid[i] == '-')
        {
            uuid[i] = '_';
        }
    }
    sprintf(random_db_name, "random_test_db_%s", uuid);

    sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS)
    {
        return 0;
    }
    sfstmt = snowflake_stmt(sf);

    sprintf(query, "create or replace database %s", random_db_name);
    status = snowflake_query(
        sfstmt,
        query,
        0);
    if (status != SF_STATUS_SUCCESS)
    {
        // ingore in case the test account doesn't have privilege to create database
        snowflake_stmt_term(sfstmt);
        snowflake_term(sf);
        return 0;
    }

    sprintf(query, "create or replace schema %s", getenv("SNOWFLAKE_TEST_SCHEMA"));
    status = snowflake_query(
        sfstmt,
        query,
        0);
    if (status != SF_STATUS_SUCCESS)
    {
        // ingore in case the test account doesn't have privilege to create database
        snowflake_stmt_term(sfstmt);
        snowflake_term(sf);
        return 0;
    }

    sf_setenv("SNOWFLAKE_TEST_DATABASE", random_db_name);
    strcpy(RANDOM_DATABASE_NAME, random_db_name);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    return 1;
}

void drop_random_database()
{
    SF_STATUS status;
    SF_CONNECT *sf;
    SF_STMT *sfstmt;
    char query[200];

    if (strlen(RANDOM_DATABASE_NAME) == 0)
    {
        // not setup
        return;
    }

    sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    sfstmt = snowflake_stmt(sf);

    sprintf(query, "drop database %s", RANDOM_DATABASE_NAME);
    status = snowflake_query(
        sfstmt,
        query,
        0);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}
