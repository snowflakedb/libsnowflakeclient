#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/test_setup.h"
#include "client_int.h"
#include "connection.h"
#include "constants.h"

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    assert_non_null(fp);
    assert_int_equal(fseek(fp, 0, SEEK_END), 0);
    long size = ftell(fp);
    assert_true(size >= 0);
    rewind(fp);
    char *buf = (char *)malloc((size_t)size + 1);
    assert_non_null(buf);
    size_t n = fread(buf, 1, (size_t)size, fp);
    buf[n] = '\0';
    fclose(fp);
    return buf;
}

static void reset_ocsp_global(void) {
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_FALSE);
}

static void restore_env_var(const char *name, const char *saved) {
    if (saved && saved[0]) {
        sf_setenv(name, saved);
    } else {
        sf_unsetenv(name);
    }
}

static void restore_log_settings(int orig_level) {
    log_set_fp(NULL);
    log_set_level(orig_level);
    log_set_quiet(SF_BOOLEAN_TRUE);
}

/* Same expression http_perform uses for CURLOPT_SSL_SF_OCSP_CHECK. */
static sf_bool http_perform_ocsp_check(sf_bool insecure_mode, sf_bool fail_open) {
    return _sf_ocsp_should_check(insecure_mode, fail_open, SF_OCSP_CHECK);
}

void test_ocsp_defaults_off(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();

    sf_bool global = SF_BOOLEAN_TRUE;
    assert_int_equal(snowflake_global_get_attribute(
                         SF_GLOBAL_OCSP_CHECK, &global, sizeof(global)),
                     SF_STATUS_SUCCESS);
    assert_int_equal(global, SF_BOOLEAN_FALSE);

    SF_CONNECT *sf = snowflake_init();
    sf_bool *fail_open = NULL;
    assert_int_equal(
        snowflake_get_attribute(sf, SF_CON_OCSP_FAIL_OPEN, (void **)&fail_open),
        SF_STATUS_SUCCESS);
    assert_int_equal(*fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);
}

void test_ocsp_get_then_set_same_value_stays_off(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    SF_CONNECT *sf = snowflake_init();
    sf_bool *fail_open = NULL;
    assert_int_equal(
        snowflake_get_attribute(sf, SF_CON_OCSP_FAIL_OPEN, (void **)&fail_open),
        SF_STATUS_SUCCESS);
    assert_int_equal(*fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(
        snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, fail_open),
        SF_STATUS_SUCCESS);
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);
}

void test_ocsp_http_perform_check_default_and_opt_in(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();

    SF_CONNECT *def = snowflake_init();
    assert_int_equal(http_perform_ocsp_check(def->insecure_mode,
                                             def->ocsp_fail_open),
                     SF_BOOLEAN_FALSE);
    assert_int_equal(http_perform_ocsp_check(def->insecure_mode,
                                             def->ocsp_fail_open),
                     _sf_ocsp_enabled(def));
    snowflake_term(def);

    SF_CONNECT *closed = snowflake_init();
    snowflake_set_attribute(closed, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_FALSE);
    assert_int_equal(http_perform_ocsp_check(closed->insecure_mode,
                                             closed->ocsp_fail_open),
                     SF_BOOLEAN_TRUE);
    assert_int_equal(http_perform_ocsp_check(closed->insecure_mode,
                                             closed->ocsp_fail_open),
                     _sf_ocsp_enabled(closed));
    snowflake_term(closed);

    sf_setenv("SF_DISABLE_OCSP_CHECKS", "false");
    SF_CONNECT *env_on = snowflake_init();
    assert_int_equal(http_perform_ocsp_check(env_on->insecure_mode,
                                             env_on->ocsp_fail_open),
                     SF_BOOLEAN_TRUE);
    assert_int_equal(http_perform_ocsp_check(env_on->insecure_mode,
                                             env_on->ocsp_fail_open),
                     _sf_ocsp_enabled(env_on));
    snowflake_term(env_on);
    sf_unsetenv("SF_DISABLE_OCSP_CHECKS");

    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_TRUE);
    sf_setenv("SF_DISABLE_OCSP_CHECKS", "true");
    SF_CONNECT *env_off = snowflake_init();
    assert_int_equal(env_off->ocsp_fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(http_perform_ocsp_check(env_off->insecure_mode,
                                             env_off->ocsp_fail_open),
                     SF_BOOLEAN_FALSE);
    assert_int_equal(http_perform_ocsp_check(env_off->insecure_mode,
                                             env_off->ocsp_fail_open),
                     _sf_ocsp_enabled(env_off));
    snowflake_term(env_off);
    sf_unsetenv("SF_DISABLE_OCSP_CHECKS");

    SF_CONNECT *insecure = snowflake_init();
    snowflake_set_attribute(insecure, SF_CON_INSECURE_MODE, &SF_BOOLEAN_TRUE);
    assert_int_equal(SF_OCSP_CHECK, SF_BOOLEAN_TRUE);
    assert_int_equal(http_perform_ocsp_check(insecure->insecure_mode,
                                             insecure->ocsp_fail_open),
                     SF_BOOLEAN_FALSE);
    assert_int_equal(http_perform_ocsp_check(insecure->insecure_mode,
                                             insecure->ocsp_fail_open),
                     _sf_ocsp_enabled(insecure));
    snowflake_term(insecure);
    reset_ocsp_global();
}

void test_ocsp_fail_open_true_is_not_opt_in(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_TRUE);
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);
}

void test_ocsp_fail_closed_opts_in(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_FALSE);
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_FALSE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_TRUE);
    snowflake_term(sf);
}

void test_ocsp_null_resets_opt_in(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_FALSE);
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, NULL);
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);
}

void test_ocsp_global_enable_fail_open_default(void **unused) {
    SF_UNUSED(unused);
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_TRUE);
    SF_CONNECT *sf = snowflake_init();
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_TRUE);
    snowflake_term(sf);
    reset_ocsp_global();
}

void test_ocsp_global_enable_plus_fail_closed(void **unused) {
    SF_UNUSED(unused);
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_TRUE);
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_FALSE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_TRUE);
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_FALSE);
    snowflake_term(sf);
    reset_ocsp_global();
}

void test_ocsp_fail_open_true_does_not_override_global_off(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_TRUE);
    assert_int_equal(SF_OCSP_CHECK, SF_BOOLEAN_FALSE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);
}

void test_ocsp_env_disable_beats_fail_open(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    sf_setenv("SF_DISABLE_OCSP_CHECKS", "true");
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);
    sf_unsetenv("SF_DISABLE_OCSP_CHECKS");
}

void test_ocsp_env_disable_does_not_beat_fail_closed(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    sf_setenv("SF_DISABLE_OCSP_CHECKS", "true");
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_FALSE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_TRUE);
    snowflake_term(sf);
    sf_unsetenv("SF_DISABLE_OCSP_CHECKS");
}

void test_ocsp_env_false_opts_in(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    sf_setenv("SF_DISABLE_OCSP_CHECKS", "false");
    SF_CONNECT *sf = snowflake_init();
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_TRUE);
    snowflake_term(sf);
    sf_unsetenv("SF_DISABLE_OCSP_CHECKS");
}

void test_ocsp_insecure_mode_wins(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_TRUE);
    snowflake_set_attribute(sf, SF_CON_INSECURE_MODE, &SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);
}

void test_ocsp_env_disable_beats_global_fail_open(void **unused) {
    SF_UNUSED(unused);
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_TRUE);
    sf_setenv("SF_DISABLE_OCSP_CHECKS", "true");
    SF_CONNECT *sf = snowflake_init();
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    assert_int_equal(_sf_ocsp_should_check(sf->insecure_mode, sf->ocsp_fail_open,
                                           SF_OCSP_CHECK),
                     SF_BOOLEAN_FALSE);
    snowflake_term(sf);
    sf_unsetenv("SF_DISABLE_OCSP_CHECKS");
    reset_ocsp_global();
}

void test_ocsp_insecure_mode_beats_global_and_fail_closed(void **unused) {
    SF_UNUSED(unused);
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_TRUE);
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &SF_BOOLEAN_FALSE);
    snowflake_set_attribute(sf, SF_CON_INSECURE_MODE, &SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    assert_int_equal(_sf_ocsp_should_check(SF_BOOLEAN_TRUE, SF_BOOLEAN_FALSE,
                                           SF_BOOLEAN_TRUE),
                     SF_BOOLEAN_FALSE);
    snowflake_term(sf);
    reset_ocsp_global();
}

void test_ocsp_dsn_fail_open_opts_in(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    SF_CONNECT *sf = snowflake_init();
    handle_single_param(sf, "OCSPFAILOPEN", "true");
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_TRUE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);
}

void test_ocsp_dsn_fail_closed_opts_in(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();
    SF_CONNECT *sf = snowflake_init();
    handle_single_param(sf, "OCSPFAILOPEN", "false");
    assert_int_equal(sf->ocsp_fail_open, SF_BOOLEAN_FALSE);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_TRUE);
    snowflake_term(sf);
}

void test_ocsp_cache_env_warns_when_off(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();

    char *orig_enabled = getenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED");
    char *orig_url = getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");
    char enabled_copy[256] = {0};
    char url_copy[4096] = {0};
    if (orig_enabled) {
        strncpy(enabled_copy, orig_enabled, sizeof(enabled_copy) - 1);
    }
    if (orig_url) {
        strncpy(url_copy, orig_url, sizeof(url_copy) - 1);
    }
    int orig_level = log_get_level();

    char logfile[512];
    snprintf(logfile, sizeof(logfile), "%s/ocsp_defaults_warn.log",
#ifdef _WIN32
             getenv("TEMP") ? getenv("TEMP") : "."
#else
             "/tmp"
#endif
    );
    remove(logfile);
    FILE *fp = fopen(logfile, "w+");
    assert_non_null(fp);
    log_set_fp(fp);
    log_set_level(SF_LOG_WARN);
    log_set_quiet(SF_BOOLEAN_TRUE);

    sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "true");
    sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL", "http://example/ocsp_response_cache.json");

    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "testaccount");
    snowflake_set_attribute(sf, SF_CON_USER, "testuser");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "testpassword");
    snowflake_set_attribute(sf, SF_CON_HOST, "testaccount.snowflakecomputing.com");
    assert_int_equal(_snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_int_equal(_sf_ocsp_enabled(sf), SF_BOOLEAN_FALSE);
    snowflake_term(sf);

    fflush(fp);
    fclose(fp);
    restore_log_settings(orig_level);

    char *contents = read_file(logfile);
    assert_non_null(strstr(contents, "SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED is ignored"));
    assert_non_null(strstr(contents, "SF_OCSP_RESPONSE_CACHE_SERVER_URL is ignored"));
    free(contents);
    remove(logfile);

    restore_env_var("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", enabled_copy);
    restore_env_var("SF_OCSP_RESPONSE_CACHE_SERVER_URL", url_copy);
}

void test_ocsp_privatelink_warns_and_skips_when_off(void **unused) {
    SF_UNUSED(unused);
    reset_ocsp_global();

    char *orig_url = getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");
    char url_copy[4096] = {0};
    if (orig_url) {
        strncpy(url_copy, orig_url, sizeof(url_copy) - 1);
    }
    int orig_level = log_get_level();
    sf_unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");

    char logfile[512];
    snprintf(logfile, sizeof(logfile), "%s/ocsp_privatelink_warn.log",
#ifdef _WIN32
             getenv("TEMP") ? getenv("TEMP") : "."
#else
             "/tmp"
#endif
    );
    remove(logfile);
    FILE *fp = fopen(logfile, "w+");
    assert_non_null(fp);
    log_set_fp(fp);
    log_set_level(SF_LOG_WARN);
    log_set_quiet(SF_BOOLEAN_TRUE);

    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "testaccount");
    snowflake_set_attribute(sf, SF_CON_USER, "testuser");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "testpassword");
    snowflake_set_attribute(sf, SF_CON_HOST, "account.privatelink.snowflakecomputing.com");
    assert_int_equal(_snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_int_equal(getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL"), NULL);
    snowflake_term(sf);

    fflush(fp);
    fclose(fp);
    restore_log_settings(orig_level);

    char *contents = read_file(logfile);
    assert_non_null(strstr(contents,
                           "Skipping privatelink SF_OCSP_RESPONSE_CACHE_SERVER_URL rewrite"));
    free(contents);
    remove(logfile);

    restore_env_var("SF_OCSP_RESPONSE_CACHE_SERVER_URL", url_copy);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    reset_ocsp_global();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ocsp_defaults_off),
        cmocka_unit_test(test_ocsp_get_then_set_same_value_stays_off),
        cmocka_unit_test(test_ocsp_http_perform_check_default_and_opt_in),
        cmocka_unit_test(test_ocsp_fail_open_true_is_not_opt_in),
        cmocka_unit_test(test_ocsp_fail_closed_opts_in),
        cmocka_unit_test(test_ocsp_null_resets_opt_in),
        cmocka_unit_test(test_ocsp_global_enable_fail_open_default),
        cmocka_unit_test(test_ocsp_global_enable_plus_fail_closed),
        cmocka_unit_test(test_ocsp_fail_open_true_does_not_override_global_off),
        cmocka_unit_test(test_ocsp_env_disable_beats_fail_open),
        cmocka_unit_test(test_ocsp_env_disable_does_not_beat_fail_closed),
        cmocka_unit_test(test_ocsp_env_false_opts_in),
        cmocka_unit_test(test_ocsp_insecure_mode_wins),
        cmocka_unit_test(test_ocsp_env_disable_beats_global_fail_open),
        cmocka_unit_test(test_ocsp_insecure_mode_beats_global_and_fail_closed),
        cmocka_unit_test(test_ocsp_dsn_fail_open_opts_in),
        cmocka_unit_test(test_ocsp_dsn_fail_closed_opts_in),
        cmocka_unit_test(test_ocsp_cache_env_warns_when_off),
        cmocka_unit_test(test_ocsp_privatelink_warns_and_skips_when_off),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
