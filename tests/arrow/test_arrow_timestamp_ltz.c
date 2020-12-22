/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include <utils/test_setup.h>

#define USER_TZ "America/New_York"

/**
 * Utility struct to help with building test cases.
 * c1, c2 and c3 refer to the columns in the test table, t, used throughout this test set.
 *
 * - c1: The positional index of the test case.
 * - c2: A pointer to an input number value to test.
 *       Note: the number is represented as a string to use arbitrary precision and scale.
 * - c2_len: The length of the input number value.
 * - c2_is_null: A flag indicating whether the input number value is NULL or not.
 * - error_code: The error code that a test case is expected to fail with.
 *     - 100108 for "Timestamp '___' is not recognized"
 */
typedef struct test_case_to_string {
    const int64 c1;
    const char * c2;
    const size_t c2_len;
    const sf_bool c2_is_null;
    const SF_STATUS error_code;
} TEST_CASE_TO_STRING;

/**
 * Test set containing all individual test cases to run.
 */
void test_arrow_timestamp_ltz_helper(sf_bool use_zero_precision) {
    // Test cases.
    const char * empty            = "";
    // Format 1: YYYY-MM-DD HH:MM:SS
    const char * f1_trash         = "ABCD-EF-GH IJ:KL:MN";
    const char * f1_neg_zero_time = "1970-01-01 -00:00:00";
    const char * f1_neg_reg_date  = "-1970-01-01 00:00:00";
    const char * f1_neg_reg_time  = "1970-01-01 -12:34:56";
    const char * f1_zero_day      = "1970-01-00 12:34:56";
    const char * f1_zero_month    = "1970-00-01 12:34:56";
    const char * f1_zero_year     = "0000-01-01 12:34:56";
    const char * f1_oob_ss        = "1970-01-01 00:00:60";
    const char * f1_oob_mm        = "1970-01-01 00:60:00";
    const char * f1_oob_hh        = "1970-01-01 24:00:00";
    const char * f1_oob_day       = "1970-01-32 00:00:00";
    const char * f1_oob_month     = "1970-13-01 00:00:00";
    const char * f1_oob_year      = "10000-01-01 00:00:00";
    const char * f1_epoch_eve     = "1969-12-31 23:59:59";
    const char * f1_epoch         = "1970-01-01 00:00:00";
    const char * f1_epochalypse   = "2038-01-19 03:14:08";
    const char * f1_min           = "0001-01-01 00:00:00";
    const char * f1_max           = "9999-12-31 23:59:59";
    const char * f1_noon          = "1970-01-01 12:00:00";
    // Format 2: YYYY-MM-DD HH:MM:SS.NNNNNNNNN
    const char * f2_trash         = "ABCD-EF-GH IJ:KL:MN.OPQRSTUVW";
    const char * f2_neg_zero_time = "1970-01-01 -00:00:00.000000000";
    const char * f2_neg_reg_date  = "-1970-01-01 12:34:56.9876543321";
    const char * f2_neg_reg_time  = "1970-01-01 -12:34:56.9987654321";
    const char * f2_zero_day      = "1970-01-00 12:34:56.123456789";
    const char * f2_zero_month    = "1970-00-01 12:34:56.123456789";
    const char * f2_zero_year     = "0000-01-01 12:34:56.123456789";
    const char * f2_oob_nano      = "1970-01-01 00:00:00.9999999999";
    const char * f2_oob_ss        = "1970-01-01 00:00:60.000000000";
    const char * f2_oob_mm        = "1970-01-01 00:60:00.000000000";
    const char * f2_oob_hh        = "1970-01-01 24:00:00.000000000";
    const char * f2_oob_day       = "1970-01-32 00:00:00.000000000";
    const char * f2_oob_month     = "1970-13-01 00:00:00.000000000";
    const char * f2_oob_year      = "10000-01-01 00:00:00.000000000";
    const char * f2_epoch_eve     = "1969-12-31 23:59:59.999999999";
    const char * f2_epoch         = "1970-01-01 00:00:00.000000000";
    const char * f2_epochalypse   = "2038-01-19 03:14:07.000000000";
    const char * f2_min           = "0001-01-01 00:00:00.000000000";
    const char * f2_max           = "9999-12-31 23:59:59.999999999";
    const char * f2_noon          = "1970-01-01 12:00:00.000000000";
    // Format 3: YYYY-MM-DD HH:MM:SS ±HH:MM
    const char * f3_neg_trash  = "1970-01-01 00:00:00 -AB:CD";
    const char * f3_pos_trash  = "1970-01-01 00:00:00 +AB:CD";
    const char * f3_oob_neg_hh = "1970-01-01 00:00:00 -13:00";
    const char * f3_oob_neg_mm = "1970-01-01 00:00:00 -12:60";
    const char * f3_oob_pos_hh = "1970-01-01 00:00:00 +15:00";
    const char * f3_oob_pos_mm = "1970-01-01 00:00:00 +14:60";
    const char * f3_dne_neg_tz = "1970-01-01 00:00:00 -01:23";
    const char * f3_dne_pos_tz = "1970-01-01 00:00:00 +01:23";
    const char * f3_bad_neg_hh = "1970-01-01 00:00:00 -1:23";
    const char * f3_bad_neg_mm = "1970-01-01 00:00:00 -01:2";
    const char * f3_bad_pos_hh = "1970-01-01 00:00:00 +1:23";
    const char * f3_bad_pos_mm = "1970-01-01 00:00:00 +01:2";
    const char * f3_past_min   = "1970-01-01 00:00:00 -12:01";
    const char * f3_past_max   = "1970-01-01 00:00:00 +14:01";
    const char * f3_min        = "1970-01-01 00:00:00 -12:00";
    const char * f3_max        = "1970-01-01 00:00:00 +14:00";
    const char * f3_reg_neg_hh = "1970-01-01 00:00:00 -03:00";
    const char * f3_reg_neg_mm = "1970-01-01 00:00:00 -03:30";
    const char * f3_reg_pos_hh = "1970-01-01 00:00:00 +03:00";
    const char * f3_reg_pos_mm = "1970-01-01 00:00:00 +03:30";
    // Format 4: YYYY-MM-DD HH:MM:SS.NNNNNNNNN ±HH:MM
    const char * f4_neg_trash  = "1970-01-01 00:00:00.000000000 -AB:CD";
    const char * f4_pos_trash  = "1970-01-01 00:00:00.000000000 +AB:CD";
    const char * f4_oob_neg_hh = "1970-01-01 00:00:00.000000000 -13:00";
    const char * f4_oob_neg_mm = "1970-01-01 00:00:00.000000000 -12:60";
    const char * f4_oob_pos_hh = "1970-01-01 00:00:00.000000000 +15:00";
    const char * f4_oob_pos_mm = "1970-01-01 00:00:00.000000000 +14:60";
    const char * f4_dne_neg_tz = "1970-01-01 00:00:00.000000000 -01:23";
    const char * f4_dne_pos_tz = "1970-01-01 00:00:00.000000000 +01:23";
    const char * f4_bad_neg_hh = "1970-01-01 00:00:00.000000000 -1:23";
    const char * f4_bad_neg_mm = "1970-01-01 00:00:00.000000000 -01:2";
    const char * f4_bad_pos_hh = "1970-01-01 00:00:00.000000000 +1:23";
    const char * f4_bad_pos_mm = "1970-01-01 00:00:00.000000000 +01:2";
    const char * f4_past_min   = "1970-01-01 00:00:00.000000000 -12:01";
    const char * f4_past_max   = "1970-01-01 00:00:00.000000000 +14:01";
    const char * f4_min        = "1970-01-01 00:00:00.000000000 -12:00";
    const char * f4_max        = "1970-01-01 00:00:00.000000000 +14:00";
    const char * f4_reg_neg_hh = "1970-01-01 00:00:00.000000000 -03:00";
    const char * f4_reg_neg_mm = "1970-01-01 00:00:00.000000000 -03:30";
    const char * f4_reg_pos_hh = "1970-01-01 00:00:00.000000000 +03:00";
    const char * f4_reg_pos_mm = "1970-01-01 00:00:00.000000000 +03:30";

    TEST_CASE_TO_STRING p0_test_cases[] = {
        { .c1 = 0,  .c2 = NULL,              .c2_len = 0,  .c2_out = NULL,                  .c2_is_null = SF_BOOLEAN_TRUE },
        { .c1 = 1,  .c2 = &empty,            .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 2,  .c2 = &f1_trash,         .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 3,  .c2 = &f1_neg_zero_time, .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 4,  .c2 = &f1_neg_reg_date,  .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 5,  .c2 = &f1_neg_reg_time,  .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 6,  .c2 = &f1_zero_day,      .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 7,  .c2 = &f1_zero_month,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 8,  .c2 = &f1_zero_year,     .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 9,  .c2 = &f1_oob_ss,        .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 10, .c2 = &f1_oob_mm,        .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 11, .c2 = &f1_oob_hh,        .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 12, .c2 = &f1_oob_day,       .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 13, .c2 = &f1_oob_month,     .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 14, .c2 = &f1_oob_year,      .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 15, .c2 = &f1_epoch_eve,     .c2_len = 19, .c2_out = "1969-12-31 23:59:59", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 16, .c2 = &f1_epoch,         .c2_len = 19, .c2_out = "1970-01-01 00:00:00", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 17, .c2 = &f1_epochalypse,   .c2_len = 19, .c2_out = "2038-01-19 03:14:08", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 18, .c2 = &f1_min,           .c2_len = 19, .c2_out = "0001-01-01 00:00:00", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 19, .c2 = &f1_max,           .c2_len = 19, .c2_out = "9999-12-31 23:59:59", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 20, .c2 = &f1_noon,          .c2_len = 19, .c2_out = "1970-01-01 12:00:00", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 21, .c2 = &f3_neg_trash,     .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 21, .c2 = &f3_pos_trash,     .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 22, .c2 = &f3_oob_neg_hh,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 23, .c2 = &f3_oob_neg_mm,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 24, .c2 = &f3_oob_pos_hh,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 25, .c2 = &f3_oob_pos_mm,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 26, .c2 = &f3_dne_neg_tz,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 27, .c2 = &f3_dne_pos_tz,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 28, .c2 = &f3_bad_neg_hh,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 29, .c2 = &f3_bad_neg_mm,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 30, .c2 = &f3_bad_pos_hh,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 31, .c2 = &f3_bad_pos_mm,    .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 32, .c2 = &f3_past_min,      .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 33, .c2 = &f3_past_max,      .c2_len = 0,  .c2_out = "",                    .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 34, .c2 = &f3_min,           .c2_len = 0,  .c2_out = "1970-01-01 07:00:00", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 35, .c2 = &f3_max,           .c2_len = 0,  .c2_out = "1969-12-31 05:00:00", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 36, .c2 = &f3_reg_neg_hh,    .c2_len = 0,  .c2_out = "1970-01-01 02:00:00", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 37, .c2 = &f3_reg_neg_mm,    .c2_len = 0,  .c2_out = "1970-01-01 02:30:00", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 38, .c2 = &f3_reg_neg_hh,    .c2_len = 0,  .c2_out = "1970-01-01 08:00:00", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 39, .c2 = &f3_reg_neg_mm,    .c2_len = 0,  .c2_out = "1970-01-01 08:30:00", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 }
    };

    TEST_CASE_TO_STRING p9_test_cases[] = {
        { .c1 =  1, .c2 = &f2_trash,         .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 =  2, .c2 = &f2_neg_zero_time, .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 =  3, .c2 = &f2_neg_reg_date,  .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 =  4, .c2 = &f2_neg_reg_time,  .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 =  5, .c2 = &f2_zero_day,      .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 =  6, .c2 = &f2_zero_month,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 =  7, .c2 = &f2_zero_year,     .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 =  8, .c2 = &f2_oob_nano,      .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 =  9, .c2 = &f2_oob_ss,        .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 10, .c2 = &f2_oob_mm,        .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 11, .c2 = &f2_oob_hh,        .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 12, .c2 = &f2_oob_day,       .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 13, .c2 = &f2_oob_month,     .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 14, .c2 = &f2_oob_year,      .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 15, .c2 = &f2_epoch_eve,     .c2_len = 29, .c2_out = "1969-12-31 23:59:59.999999999", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 16, .c2 = &f2_epoch,         .c2_len = 29, .c2_out = "1970-01-01 00:00:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 17, .c2 = &f2_epochalypse,   .c2_len = 29, .c2_out = "2038-01-19 03:14:08.000000000", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 18, .c2 = &f2_min,           .c2_len = 29, .c2_out = "0001-01-01 00:00:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 19, .c2 = &f2_max,           .c2_len = 29, .c2_out = "9999-12-31 23:59:59.999999999", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 20, .c2 = &f2_noon,          .c2_len = 29, .c2_out = "1970-01-01 12:00:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 21, .c2 = &f4_neg_trash,     .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 21, .c2 = &f4_pos_trash,     .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 22, .c2 = &f4_oob_neg_hh,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 23, .c2 = &f4_oob_neg_mm,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 24, .c2 = &f4_oob_pos_hh,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 25, .c2 = &f4_oob_pos_mm,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 26, .c2 = &f4_dne_neg_tz,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 27, .c2 = &f4_dne_pos_tz,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 28, .c2 = &f4_bad_neg_hh,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 29, .c2 = &f4_bad_neg_mm,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 30, .c2 = &f4_bad_pos_hh,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 31, .c2 = &f4_bad_pos_mm,    .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 32, .c2 = &f4_past_min,      .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 33, .c2 = &f4_past_max,      .c2_len = 0,  .c2_out = "",                              .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 34, .c2 = &f4_min,           .c2_len = 0,  .c2_out = "1970-01-01 07:00:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 35, .c2 = &f4_max,           .c2_len = 0,  .c2_out = "1969-12-31 05:00:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 36, .c2 = &f4_reg_neg_hh,    .c2_len = 0,  .c2_out = "1970-01-01 02:00:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 37, .c2 = &f4_reg_neg_mm,    .c2_len = 0,  .c2_out = "1970-01-01 02:30:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 38, .c2 = &f4_reg_neg_hh,    .c2_len = 0,  .c2_out = "1970-01-01 08:00:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 39, .c2 = &f4_reg_neg_mm,    .c2_len = 0,  .c2_out = "1970-01-01 08:30:00.000000000", .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 }
    };

    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Create a statement object and reuse it for all test cases.
    SF_STMT *sfstmt = snowflake_stmt(sf);

    // Configure this session to use Arrow format.
    status = snowflake_query(
        sfstmt,
        "alter session set ODBC_QUERY_RESULT_FORMAT=ARROW_FORCE",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Configure Timestamp LTZ output format depending on precision.
    status = snowflake_query(
        sfstmt,
        use_zero_precision ?
            "alter session set TIMESTAMP_LTZ_OUTPUT_FORMAT='YYYY-MM-DD HH24:MI:SS'" :
            "alter session set TIMESTAMP_LTZ_OUTPUT_FORMAT='YYYY-MM-DD HH24:MI:SS.FF9'",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(
        sfstmt,
        use_zero_precision ?
            "create or replace table t (c1 int, c2 timestamp_ltz(0))" :
            "create or replace table t (c1 int, c2 timestamp_ltz(9))",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_prepare(
        sfstmt,
        "insert into t(c1,c2) values(?,?)",
        0);

    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Loop over the test cases via index and insert them one by one.
    TEST_CASE_TO_STRING[] test_cases =
        use_zero_precision ? p0_test_cases : p9_test_cases;
    size_t i;
    size_t num_test_cases = sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING);

    for (i = 0; i < num_test_cases; i++) {
        TEST_CASE_TO_STRING tc = test_cases[i];

        // Bind input c1.
        SF_BIND_INPUT in_c1 = { 0 };
        in_c1.idx = 1;
        in_c1.name = NULL;
        in_c1.c_type = SF_C_TYPE_INT64;
        in_c1.value = (void*) &tc.c1;
        in_c1.len = sizeof(tc.c1);

        status = snowflake_bind_param(sfstmt, &in_c1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Bind input c2.
        SF_BIND_INPUT in_c2 = { 0 };
        in_c2.idx = 2;
        in_c2.name = NULL;
        in_c2.c_type = SF_C_TYPE_STRING;
        in_c2.value = (void*) tc.c2;
        in_c2.len = tc.c2_len;

        status = snowflake_bind_param(sfstmt, &in_c1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Execute.
        // If the error_code member is non-zero, then we expect the query to fail.
        // In that case, ensure the error code matches with what is expected.
        // Otherwise, proceed as normal.
        int num_successful_inserts = 0;
        status = snowflake_execute(sfstmt);
        if (tc.error_code == SF_STATUS_SUCCESS) {
            if (status != SF_STATUS_SUCCESS) {
                dump_error(&(sfstmt->error));
            }
            ++num_successful_inserts;
        } else {
            SF_ERROR_STRUCT * err = snowflake_stmt_error(sfstmt);
            assert_int_equal(tc.error_code, error->error_code);
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
    }

    // Query the table and check for correctness.
    status = snowflake_query(sfstmt, "select * from t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), num_successful_inserts);

    int64 c1 = 0;
    char *c2 = NULL;
    size_t c2_len = 0;
    size_t c2_max_size = 0;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &c1);
        TEST_CASE_TO_STRING tc = test_cases[c1];

        // Valid if value copied to c2 matches the value in tc.c2.
        // c2_len and c2_max_size are unused.
        if (tc.c2 != NULL) {
            snowflake_column_as_str(sfstmt, 2, &c2, &c2_len, &c2_max_size);
            assert_string_equal(tc.c2_out, c2);
        } else {
            sf_bool is_null;
            snowflake_column_is_null(stmt, 2, is_null);
            assert_true(tc.c2_is_null == is_null);
        }
    }

    // Clean-up.
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);

    status = snowflake_query(sfstmt, "drop table if exists t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    free(c2);
    c2 = NULL;
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_arrow_timestamp_ltz(void** unused) {
    test_arrow_timestamp_ltz_helper(SF_BOOLEAN_TRUE);
    test_arrow_timestamp_ltz_helper(SF_BOOLEAN_FALSE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = { cmocka_unit_test(test_arrow_timestamp_ltz) };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
