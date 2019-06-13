/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <ctype.h>

#include <string.h>
#include <unistd.h>
#include <snowflake/client.h>
#include "utils/test_setup.h"

#define STRING_FIELD_MAX_SIZE 1048576
#define STRING_FIELD_FIXED_SIZE 10240

const char *COL_EVAL_QUERY = "select randstr(10239,random()) from table(generator(rowcount=>500));";
const size_t NUM_COLS = 1;
const int NUM_ROWS = 500;

typedef struct BOOK {
    char *title;
    char *text;
    size_t text_len;
} BOOK;

void test_col_string_read_fixed_size(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, COL_EVAL_QUERY);

    // Begin timing
    clock_gettime(clk_id, &begin);

    size_t len;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_strlen(sfstmt, 1, &len);
        if (len != STRING_FIELD_FIXED_SIZE - 1) {
            fail_msg("Query in %s should have a text field with %i characters, instead has %zd", "test_col_string_read_fixed_size", STRING_FIELD_FIXED_SIZE - 1, len);
        }
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, NUM_ROWS, "test_col_string_read_fixed_size");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_string_manipulate_fixed_size(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, COL_EVAL_QUERY);

    // Begin timing
    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    char *out = NULL;

    size_t len;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Convert string to lowercase then discard
        snowflake_column_as_str(sfstmt, 1, &out, NULL, NULL);
        char *p = out;
        while (*p) {
            *p = (char) tolower(*p);
            p++;
        }
        free(out);
        out = NULL;
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, NUM_ROWS, "test_col_string_manipulate_fixed_size");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_buffer_copy_unknown_size_dynamic_memory(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    // Randomly select string size based on provided gen seed
    setup_and_run_query(&sf, &sfstmt,
                        "select randstr(uniform(1, 1048575, 8888),random()) from table(generator(rowcount=>500));");

    // Begin timing
    clock_gettime(clk_id, &begin);

    int row = 0;
    // Create array of char pointers to hold dynamically created buffers
    char *out_buff[NUM_ROWS];
    char *out = NULL;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Create dynamic buffer and copy over
        snowflake_column_as_str(sfstmt, 1, &out, NULL, NULL);
        out_buff[row] = out;
        out = NULL;
        row++;
    }

    for (int i = 0; i < row; i++) {
        free(out_buff[i]);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, NUM_ROWS, "test_col_buffer_copy_unknown_size_dynamic_memory");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_buffer_copy_concat_multiple_rows(void **unused) {
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;
    BOOK books[5];

    // Setup books structs
    for (int i = 0; i < 5; i++) {
        books[i].title = NULL;
        books[i].text = NULL;
        books[i].text_len = 0;
    }

    // Get all public domain books. Sort by text_part_id to ensure that you concatenate book in right order
    setup_and_run_query(&sf, &sfstmt, "select * from public_domain_books order by id, text_part;");
    // Begin timing
    clock_gettime(clk_id, &begin);

    int row = 0;
    // Store ID to use as array index
    int64 id = 0;
    // Use max buffer size since we have to assume the largest buffer size for each row of text
    char *title = NULL;
    // Use max buffer size since we have to assume the largest buffer size for each row of text
    const char *text = NULL;
    size_t text_len = 0;

    while (snowflake_fetch(sfstmt) == SF_STATUS_SUCCESS) {
        // Set title if NULL
        if (!books[id].title) {
            snowflake_column_as_str(sfstmt, 2, &title, NULL, NULL);
            books[id].title = title;
            title = NULL;
        }
        // Copy text into the right buffer
        snowflake_column_as_const_str(sfstmt, 3, &text);
        snowflake_column_strlen(sfstmt, 3, &text_len);
        books[id].text = (char *) realloc(books[id].text, text_len + books[id].text_len + 1);
        memcpy(&books[id].text[books[id].text_len], text, text_len);
        books[id].text_len += text_len;
        books[id].text[books[id].text_len] = '\0';
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 1, "test_col_buffer_copy_concat_multiple_rows");

    // Free all memory associated with the book.
    for (int i = 0; i < 5; i++) {
        if (books[i].title) {
            free(books[i].title);
            books[i].title = NULL;
        }
        if (books[i].text) {
            free(books[i].text);
            books[i].text = NULL;
        }
    }

    if (title) {
        free(title);
    }
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_col_string_read_fixed_size),
      cmocka_unit_test(test_col_string_manipulate_fixed_size),
      cmocka_unit_test(test_col_buffer_copy_unknown_size_dynamic_memory),
      cmocka_unit_test(test_col_buffer_copy_concat_multiple_rows),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}

