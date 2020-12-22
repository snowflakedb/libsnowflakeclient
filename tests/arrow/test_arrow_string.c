/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include <utils/test_setup.h>

/**
 * Utility struct to help with building test cases.
 * c1, c2 and c3 refer to the columns in the test table, t, used throughout this test set.
 *
 * - c1: The positional index of the test case.
 * - c2: A pointer to an input number value to test.
 *       Note: the number is represented as a string to use arbitrary precision and scale.
 * - c2_len: The length of the input number value.
 * - c2_is_null: A flag indicating whether the input number value is NULL or not.
 * - error_code: The error code a test case is expected to fail with.
 *     - ?????? for "String value '___' is not recognized"
 *     - ?????? for "SQL compilation error: syntax error line L at position P unexpected '___'"
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
void test_arrow_string(void **unused) {
    // Test cases.
    const char * empty        = "";
    const char * large_string = "";
    // Escape sequences.
    // Note: Skipping formfeed sequence.
    const char * tab              = "Separated\tby\ttab\tsequence\t\\t";
    const char * unix_linefeed    = "Separated\nby\nUNIX\nlinefeeds\n\\n";
    const char * windows_linefeed = "Separated\r\nby\r\nWindows\r\nlinefeeds\r\n\\r\\n";
    const char * backspace        = "Bkac\b\b\backsap\b\bpace";
    const char * backspace_out    = "Backspace";
    const char * backslash        = "\\Testing\\\\backslash\\only";
    const char * mixed_backslash  = "\\Testing\\\b\\backslash\t\\and\\\r\n\\sequences";
    const char * octal            = "\252\253\254\255\256\257";
    const char * hex              = "\xAA\xAB\xAC\xAD\xAE\xAF";
    const char * octal_hex_out    = "ª«¬­®¯";
    // Quotations.
    // Note: Skipping dollar-quoted string constants.
    const char * single_quote            = "'Unescaped single-quotes should fail'";
    const char * double_single_quote     = "''Single-quotes escaped by doubling''";
    const char * double_single_quote_out = "'Single-quotes escaped by doubling'";
    const char * escape_single_quote     = "\'Single-quotes escaped with backslash\'";
    const char * escape_single_quote_out = "'Single-quotes escaped with backslash'";
    const char * double_quote            = "\"Double-quotes escaped with backslash\"";
    // Special characters.
    const char * ascii_punc       = ",./;\'[]\\-=<>?:\"{}|_+!@#$%^&*()`~";
    const char * unicode_a        = "Ω≈ç√∫˜µ≤≥÷åß∂ƒ©˙∆˚¬…æœ∑´®†¥¨ˆøπ“‘¡™£¢∞§¶•ªº–≠`⁄€‹›ﬁﬂ‡°·‚—±";
    const char * unicode_b        = "ЁЂЃЄЅІЇЈЉЊЋЌЍЎЏАБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдежзийклмнопрстуфхцчшщъыьэюя";
    const char * unicode_c        = "表ポあA鷗ŒéＢ逍Üßªąñ丂㐀𠀀";
    const char * two_byte_char_cn = "𠜎𠜱𠝹𠱓𠱸𠲖𠳏";
    const char * two_byte_char_jp = "田中さんにあげて下さい";
    const char * two_byte_char_kr = "찦차를 타고 온 펲시맨과 쑛다리 똠방각하";
    const char * two_byte_letter  = "𐐜 𐐔𐐇𐐝𐐀𐐡𐐇𐐓 𐐙𐐊𐐡𐐝𐐓/𐐝𐐇𐐗𐐊𐐤𐐔 𐐒𐐋𐐗 𐐒𐐌 𐐜 𐐡𐐀𐐖𐐇𐐤𐐓𐐝 𐐱𐑂 𐑄 𐐔𐐇𐐝𐐀𐐡𐐇𐐓 𐐏𐐆𐐅𐐤𐐆𐐚𐐊𐐡𐐝𐐆𐐓𐐆";
    const char * kaomoji          = "(╯°□°）╯︵ ┻━┻)";
    const char * emoji            = "🐵 🙈 🙉 🙊";
    // Injection.
    const char * sql_injection_a = "1;DROP TABLE t";
    const char * sql_injection_b = "1'; DROP TABLE t-- 1";
    const char * sql_injection_c = "' OR 1=1 -- 1";
    const char * sql_injection_d = "' OR '1'='1";

    TEST_CASE_TO_STRING test_cases[] = {
        { .c1 = 0,  .c2 = NULL,                 .c2_len = 0,  .c2_out = NULL,                     .c2_is_null = SF_BOOLEAN_TRUE },
        { .c1 = 1,  .c2 = &empty,               .c2_len = 0,  .c2_out = "",                       .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 2,  .c2 = &tab,                 .c2_len = 0,  .c2_out = &tab,                     .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 3,  .c2 = &unix_linefeed,       .c2_len = 0,  .c2_out = &unix_linefeed,           .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 4,  .c2 = &windows_linefeed,    .c2_len = 0,  .c2_out = &windows_linefeed,        .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 5,  .c2 = &backspace,           .c2_len = 0,  .c2_out = &backspace_out,           .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 6,  .c2 = &backslash,           .c2_len = 0,  .c2_out = &backslash,               .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 7,  .c2 = &mixed_backslash,     .c2_len = 0,  .c2_out = &mixed_backslash,         .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 8,  .c2 = &octal,               .c2_len = 0,  .c2_out = &octal_hex_out,           .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 9,  .c2 = &hex,                 .c2_len = 0,  .c2_out = &octal_hex_out,           .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 10, .c2 = &single_quote,        .c2_len = 0,  .c2_out = "",                       .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 999999 },
        { .c1 = 11, .c2 = &double_single_quote, .c2_len = 0,  .c2_out = &double_single_quote_out, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 12, .c2 = &escape_single_quote, .c2_len = 0,  .c2_out = &escape_single_quote_out, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 13, .c2 = &double_quote,        .c2_len = 0,  .c2_out = &double_quote,            .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 14, .c2 = &ascii_punc,          .c2_len = 0,  .c2_out = &ascii_punc,              .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 15, .c2 = &unicode_a,           .c2_len = 0,  .c2_out = &unicode_a,               .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 16, .c2 = &unicode_b,           .c2_len = 0,  .c2_out = &unicode_b,               .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 17, .c2 = &unicode_c,           .c2_len = 0,  .c2_out = &unicode_c,               .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 18, .c2 = &two_byte_char_cn,    .c2_len = 0,  .c2_out = &two_byte_char_cn,        .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 19, .c2 = &two_byte_char_jp,    .c2_len = 0,  .c2_out = &two_byte_char_jp,        .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 20, .c2 = &two_byte_char_kr,    .c2_len = 0,  .c2_out = &two_byte_char_kr,        .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 21, .c2 = &two_byte_letter,     .c2_len = 0,  .c2_out = &two_byte_letter,         .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 22, .c2 = &kaomoji,             .c2_len = 0,  .c2_out = &kaomoji,                 .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 23, .c2 = &emoji,               .c2_len = 0,  .c2_out = &emoji,                   .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 24, .c2 = &sql_injection_a,     .c2_len = 0,  .c2_out = &sql_injection_a,         .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 25, .c2 = &sql_injection_b,     .c2_len = 0,  .c2_out = "",                       .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 999999 },
        { .c1 = 26, .c2 = &sql_injection_c,     .c2_len = 0,  .c2_out = "",                       .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 999999 },
        { .c1 = 27, .c2 = &sql_injection_d,     .c2_len = 0,  .c2_out = "",                       .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 999999 },
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

    // NOTE: The numeric type here should fit into int64.
    // Otherwise, it is taken as a float.
    status = snowflake_query(
        sfstmt,
        "create or replace table t (c1 int, c2 string)",
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

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = { cmocka_unit_test(test_arrow_string) };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
