#include <string.h>
#include "utils/test_setup.h"
#include "../lib/snowflake_util.h"
#include <stdint.h>


void test_parse_bool(void** unused) {
    sf_bool out;
    assert_true(parse_bool("true", &out));
    assert_true(out);
    assert_true(parse_bool("1", &out));
    assert_true(out);
    assert_true(parse_bool("false", &out));
    assert_false(out);
    assert_true(parse_bool("0", &out));
    assert_false(out);
    assert_false(parse_bool("notabool", &out));
}

void test_parse_int64(void** unused) {
    int64 out;
    assert_true(parse_int64("123", &out));
    assert_int_equal(out, 123);
    assert_true(parse_int64("-123", &out));
    assert_int_equal(out, -123);
    assert_false(parse_int64("notanint", &out));
    assert_false(parse_int64("123abc", &out));
    assert_false(parse_int64("", &out));

}

void test_parse_int8(void** unused) {
    int8 out;
    assert_true(parse_int8("123", &out));
    assert_int_equal(out, 123);
    assert_true(parse_int8("-123", &out));
    assert_int_equal(out, -123);
    assert_false(parse_int8("notanint", &out));
    assert_false(parse_int8("123abc", &out));
    assert_false(parse_int8("", &out));
    assert_false(parse_int8("128", &out));
    assert_false(parse_int8("-129", &out));
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_parse_bool),
      cmocka_unit_test(test_parse_int64),
      cmocka_unit_test(test_parse_int8),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
