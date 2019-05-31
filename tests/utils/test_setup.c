/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <snowflake/logger.h>
#include <string.h>
#include "test_setup.h"

// Long path space
char PERFORMANCE_TEST_RESULTS_PATH[4096];

void initialize_test(sf_bool debug) {
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
    if (!test_log_dir) {
        char tmp_dir[4096];
        sf_get_tmp_dir(tmp_dir);
        snprintf(PERFORMANCE_TEST_RESULTS_PATH, 4096, "%s%s%s", tmp_dir, sep, perf_test_fn);
    } else {
        snprintf(PERFORMANCE_TEST_RESULTS_PATH, 4096, "%s%s%s", test_log_dir, sep, perf_test_fn);
    }
}

SF_CONNECT *setup_snowflake_connection() {
    return setup_snowflake_connection_with_autocommit(
            "UTC", SF_BOOLEAN_TRUE);
}

SF_CONNECT *setup_snowflake_connection_with_autocommit(
        const char *timezone, sf_bool autocommit) {
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
    if (host) {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port) {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol) {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }
    return sf;
}

void dump_error(SF_ERROR_STRUCT *error) {
    fprintf(stderr, "Error code: %d, message: %s\nIn File, %s, Line, %d\n",
            error->error_code,
            error->msg,
            error->file,
            error->line);
}

void setup_and_run_query(SF_CONNECT **sfp, SF_STMT **sfstmtp, const char *query) {
    SF_STATUS status;
    SF_CONNECT *sf;
    SF_STMT *sfstmt;
    sf = setup_snowflake_connection();
    // Save created connection
    *sfp = sf;
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
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
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
}
void process_results(struct timespec begin, struct timespec end, int num_iterations, const char *label) {
    FILE *results_file = fopen(PERFORMANCE_TEST_RESULTS_PATH, "a+");
    double time_elapsed = (double) (end.tv_sec - begin.tv_sec) + (double) (end.tv_nsec - begin.tv_nsec) / 1000000000;
    fprintf(results_file, "%s, %lf, %i\n", label, time_elapsed, num_iterations);
    //printf("%s, %lf, %i\n", label, time_elapsed, num_iterations);
    fclose(results_file);
}
