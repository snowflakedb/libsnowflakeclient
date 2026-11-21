#include <string.h>
#include "utils/test_setup.h"
#include "../lib/snowflake_util.h"
#include "../lib/client_int.h"
#include <stdint.h>

void test_parse_bool(void** unused) {
    SF_UNUSED(unused);

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
    SF_UNUSED(unused);

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
    SF_UNUSED(unused);

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

void test_is_put_get_command(void** unused) {
    SF_UNUSED(unused);

    /* Valid PUT and GET commands. */
    assert_true(_is_put_get_command("put file:///tmp/file @stage "));
    assert_true(_is_put_get_command("PUT file:///tmp/file @stage "));
    assert_true(_is_put_get_command("get @stage file:///tmp "));
    assert_true(_is_put_get_command("GET @stage file:///tmp "));

    /* Leading spaces, block comments are supported. */
    assert_true(_is_put_get_command("   put file:///tmp/file @stage "));
    assert_true(_is_put_get_command("/* comment */ put file:///tmp/file @stage "));
    assert_true(_is_put_get_command("/* comment */ /* another */ get @stage file:///tmp "));

    /* New lines are supported */
    assert_true(_is_put_get_command("put file:///tmp/file @stage\n"));
    assert_true(_is_put_get_command("get \n file:///tmp/file \n @stage"));

    /* Non-PUT/GET statements must not match. */
    assert_false(_is_put_get_command("select * from put"));
    assert_false(_is_put_get_command("insert into get values (1)"));

    /* PUT/GET must be followed by whitespace. */
    assert_false(_is_put_get_command("put_file:///tmp/file"));
    assert_false(_is_put_get_command("get_stage"));
    assert_false(_is_put_get_command("put"));
    assert_false(_is_put_get_command("get"));
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_parse_bool),
      cmocka_unit_test(test_parse_int64),
      cmocka_unit_test(test_parse_int8),
      cmocka_unit_test(test_is_put_get_command),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
