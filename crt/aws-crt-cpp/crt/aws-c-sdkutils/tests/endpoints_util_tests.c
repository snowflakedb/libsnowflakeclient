/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <aws/common/byte_buf.h>
#include <aws/common/string.h>
#include <aws/sdkutils/private/endpoints_util.h>
#include <aws/testing/aws_test_harness.h>

AWS_TEST_CASE(endpoints_eval_util_is_ipv4, s_test_is_ipv4)
static int s_test_is_ipv4(struct aws_allocator *allocator, void *ctx) {
    (void)allocator;
    (void)ctx;

    ASSERT_TRUE(aws_is_ipv4(aws_byte_cursor_from_c_str("0.0.0.0")));
    ASSERT_TRUE(aws_is_ipv4(aws_byte_cursor_from_c_str("127.0.0.1")));
    ASSERT_TRUE(aws_is_ipv4(aws_byte_cursor_from_c_str("255.255.255.255")));
    ASSERT_TRUE(aws_is_ipv4(aws_byte_cursor_from_c_str("192.168.1.1")));

    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("256.0.0.1")));
    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("127.0.0")));
    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("127.0")));
    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("127")));
    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("")));

    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("foo.com")));
    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("a.b.c.d")));
    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("a127.0.0.1")));
    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("127.0.0.1a")));
    ASSERT_FALSE(aws_is_ipv4(aws_byte_cursor_from_c_str("127.0.0.1011")));

    return AWS_OP_SUCCESS;
}

AWS_TEST_CASE(endpoints_eval_util_is_ipv6, s_test_is_ipv6)
static int s_test_is_ipv6(struct aws_allocator *allocator, void *ctx) {
    (void)allocator;
    (void)ctx;

    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("0:0:0000:0000:0000:0:0:0"), false));
    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("2001:0db8:0000:0000:0000:8a2e:0370:7334"), false));
    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("2001:0DB8:0000:0000:0000:8a2e:0370:7334"), false));
    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("fe80::1"), false));
    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("fe80::1%en0"), false));
    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("[2001:0db8:0000:0000:0000:8a2e:0370:7334]"), true));
    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("[fe80::1]"), true));
    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("[fe80::1%25en0]"), true));
    ASSERT_TRUE(aws_is_ipv6(aws_byte_cursor_from_c_str("[2001:db8:85a3:8d3:1319:8a2e:370:7348]"), true));

    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("2001:0db8:0000:0000:0000:8a2e:0370"), false));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("2001:0db8:0000:0000:0000:8a2e:0370:"), false));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("2001::"), false));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("2001:0db8:0000:0000:0000:8a2e:0370:7334:8745"), false));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str(":2001:0db8:0000:0000:0000:8a2e:0370:7334:8745"), false));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("z001:0db8:0000:0000:0000:8a2e:0370:7334:8745"), false));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("z001::8a2e::8745"), false));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("::2001:0db8:0000:0000:8a2e:0370:7334"), false));

    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("fe80::1%25en0"), true));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("[fe80::1%en0]"), true));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("[fe80::1%24en0]"), true));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("[fe80::1%25en0"), true));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("fe80::1%25en0]"), true));
    ASSERT_FALSE(aws_is_ipv6(aws_byte_cursor_from_c_str("[fe80::1%25]"), true));

    return AWS_OP_SUCCESS;
}

AWS_TEST_CASE(endpoints_map_region_to_partition, s_test_map_region_to_partition)
static int s_test_map_region_to_partition(struct aws_allocator *allocator, void *ctx) {
    (void)allocator;
    (void)ctx;

    struct aws_byte_cursor partition1 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("us-west-2"));
    ASSERT_TRUE(aws_byte_cursor_eq_c_str(&partition1, "aws"));

    struct aws_byte_cursor partition2 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("us-gov-west-1"));
    ASSERT_TRUE(aws_byte_cursor_eq_c_str(&partition2, "aws-us-gov"));

    struct aws_byte_cursor partition3 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("cn-north-1"));
    ASSERT_TRUE(aws_byte_cursor_eq_c_str(&partition3, "aws-cn"));

    struct aws_byte_cursor partition4 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("us-iso-rand-1"));
    ASSERT_TRUE(aws_byte_cursor_eq_c_str(&partition4, "aws-iso"));

    struct aws_byte_cursor partition5 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("us-isob-rand-1"));
    ASSERT_TRUE(aws_byte_cursor_eq_c_str(&partition5, "aws-iso-b"));

    struct aws_byte_cursor partition6 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("af-west-1"));
    ASSERT_TRUE(aws_byte_cursor_eq_c_str(&partition6, "aws"));

    struct aws_byte_cursor partition7 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("us-west"));
    ASSERT_TRUE(partition7.len == 0);

    struct aws_byte_cursor partition8 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("us-west-a"));
    ASSERT_TRUE(partition8.len == 0);

    struct aws_byte_cursor partition9 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("zz-west-1"));
    ASSERT_TRUE(partition9.len == 0);

    struct aws_byte_cursor partition10 = aws_map_region_to_partition(aws_byte_cursor_from_c_str("us-"));
    ASSERT_TRUE(partition10.len == 0);

    return AWS_OP_SUCCESS;
}

AWS_TEST_CASE(endpoints_uri_normalize_path, s_test_uri_normalize_path)
static int s_test_uri_normalize_path(struct aws_allocator *allocator, void *ctx) {
    (void)ctx;

    struct aws_byte_buf buf1;
    ASSERT_SUCCESS(aws_byte_buf_init_from_normalized_uri_path(allocator, aws_byte_cursor_from_c_str("/"), &buf1));
    ASSERT_TRUE(aws_byte_buf_eq_c_str(&buf1, "/"));
    aws_byte_buf_clean_up(&buf1);

    struct aws_byte_buf buf2;
    ASSERT_SUCCESS(aws_byte_buf_init_from_normalized_uri_path(allocator, aws_byte_cursor_from_c_str("aaa"), &buf2));
    ASSERT_TRUE(aws_byte_buf_eq_c_str(&buf2, "/aaa/"));
    aws_byte_buf_clean_up(&buf2);

    struct aws_byte_buf buf3;
    ASSERT_SUCCESS(aws_byte_buf_init_from_normalized_uri_path(allocator, aws_byte_cursor_from_c_str("aaa/"), &buf3));
    ASSERT_TRUE(aws_byte_buf_eq_c_str(&buf3, "/aaa/"));
    aws_byte_buf_clean_up(&buf3);

    struct aws_byte_buf buf4;
    ASSERT_SUCCESS(aws_byte_buf_init_from_normalized_uri_path(allocator, aws_byte_cursor_from_c_str("/aaa"), &buf4));
    ASSERT_TRUE(aws_byte_buf_eq_c_str(&buf4, "/aaa/"));
    aws_byte_buf_clean_up(&buf4);

    struct aws_byte_buf buf5;
    ASSERT_SUCCESS(aws_byte_buf_init_from_normalized_uri_path(allocator, aws_byte_cursor_from_c_str(""), &buf5));
    ASSERT_TRUE(aws_byte_buf_eq_c_str(&buf5, "/"));
    aws_byte_buf_clean_up(&buf5);

    return AWS_OP_SUCCESS;
}

int s_resolve_cb(struct aws_byte_cursor template, void *user_data, struct aws_owning_cursor *out_resolved) {
    (void)template;
    (void)user_data;
    *out_resolved = aws_endpoints_non_owning_cursor_create(aws_byte_cursor_from_c_str("test"));
    return AWS_OP_SUCCESS;
}

AWS_TEST_CASE(
    endpoints_byte_buf_init_from_resolved_templated_string,
    s_test_byte_buf_init_from_resolved_templated_string)
static int s_test_byte_buf_init_from_resolved_templated_string(struct aws_allocator *allocator, void *ctx) {
    (void)ctx;

    struct aws_byte_buf buf;

    ASSERT_SUCCESS(aws_byte_buf_init_from_resolved_templated_string(
        allocator, &buf, aws_byte_cursor_from_c_str("{e} a {b}{c} a {d}"), s_resolve_cb, NULL, false));
    ASSERT_CURSOR_VALUE_CSTRING_EQUALS(aws_byte_cursor_from_buf(&buf), "test a testtest a test");
    aws_byte_buf_clean_up(&buf);

    ASSERT_SUCCESS(aws_byte_buf_init_from_resolved_templated_string(
        allocator,
        &buf,
        aws_byte_cursor_from_c_str("{ \"a\": \"{b} {d} \", \"c\": \" {e} \"}"),
        s_resolve_cb,
        NULL,
        true));
    ASSERT_CURSOR_VALUE_CSTRING_EQUALS(aws_byte_cursor_from_buf(&buf), "{ \"a\": \"test test \", \"c\": \" test \"}");
    aws_byte_buf_clean_up(&buf);

    ASSERT_SUCCESS(aws_byte_buf_init_from_resolved_templated_string(
        allocator, &buf, aws_byte_cursor_from_c_str("a \" {b} \" a"), s_resolve_cb, NULL, false));
    ASSERT_CURSOR_VALUE_CSTRING_EQUALS(aws_byte_cursor_from_buf(&buf), "a \" test \" a");
    aws_byte_buf_clean_up(&buf);

    ASSERT_SUCCESS(aws_byte_buf_init_from_resolved_templated_string(
        allocator, &buf, aws_byte_cursor_from_c_str("{ \"a\": \"a \\\" {b} \\\" a\" }"), s_resolve_cb, NULL, true));
    ASSERT_CURSOR_VALUE_CSTRING_EQUALS(aws_byte_cursor_from_buf(&buf), "{ \"a\": \"a \\\" test \\\" a\" }");
    aws_byte_buf_clean_up(&buf);

    return AWS_OP_SUCCESS;
}
