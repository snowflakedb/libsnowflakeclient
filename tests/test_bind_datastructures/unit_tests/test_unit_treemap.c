/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "treemap.h"
#include "memory.h"

#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>

void test_init( void **unused)
{
    TREE_MAP * test_map = sf_treemap_init();
    assert_non_null(test_map);;
    SF_FREE(test_map);
}


void test_set()
{
    char * test_param = "Test Param";
    TREE_MAP *test_map = sf_treemap_init();
    assert_int_equal(sf_treemap_set(test_map, test_param, "test_node"), SF_INT_RET_CODE_SUCCESS);
    assert_int_equal(sf_treemap_set(test_map, test_param, "test_node"), SF_INT_RET_CODE_DUPLICATES);
    sf_treemap_deallocate(test_map);
}

void test_set_bulk()
{
    int i = 0;
    TREE_MAP *test_map = sf_treemap_init();
    char **test_param = NULL;
    char **test_node = NULL;
    test_param = (char **)SF_CALLOC(10000, sizeof(char *));
    test_node = (char **)SF_CALLOC(10000,sizeof(char *));
    char *test_param_ret = NULL;
    for (i = 0; i < 10000; i++)
    {
        test_param[i] = (char *)SF_CALLOC(1,sizeof("TEST_PARAM")+sizeof(i));
        test_node[i] = (char *)SF_CALLOC(1,sizeof("TEST_NODE")+sizeof(i));
        sprintf(test_param[i],"%s%d","TEST_PARAM",i);
        sprintf(test_node[i],"%s%d","TEST_NODE",i);
        assert_int_equal(sf_treemap_set(test_map,test_param[i],test_node[i]),
                         SF_INT_RET_CODE_SUCCESS);
    }
    for (i = 0; i < 10000; i++)
    {
        test_param_ret = (char *)sf_treemap_get(test_map, test_node[i]);
        assert_non_null(test_param_ret);
        assert_string_equal(test_param_ret, test_param[i]);
    }
    for (i = 0; i < 10000; i++)
    {
        SF_FREE(test_param[i]);
        SF_FREE(test_node[i]);
    }
    SF_FREE(test_param);
    SF_FREE(test_node);
    sf_treemap_deallocate(test_map);
}

void test_get()
{
    char * test_param = "Test Param";
    char * test_param_ret = NULL;
    TREE_MAP *test_map = sf_treemap_init();
    sf_treemap_set(test_map, test_param, "test_node");
    test_param_ret = (char *)sf_treemap_get(test_map, "test_node");
    assert_string_equal(test_param_ret, test_param);
    assert_null(sf_treemap_get(test_map, "absent_node"));
    sf_treemap_deallocate(test_map);
}

int main (void) {
    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_init),
                    cmocka_unit_test(test_set),
                    cmocka_unit_test(test_set_bulk),
                    cmocka_unit_test(test_get)
            };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
}