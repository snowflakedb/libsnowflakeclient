/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "rbtree.h"
#include "memory.h"

#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>

void test_init( void **unused)
{
    RedBlackTree * test_tree = rbtree_init();
    assert_non_null(test_tree);
    assert_null(test_tree->key);
    assert_null(test_tree->elem);
    assert_null(test_tree->left);
    assert_null(test_tree->right);
    assert_null(test_tree->parent);
    assert_int_equal(test_tree->color, BLACK);
    SF_FREE(test_tree);
}

void test_insert()
{
    char * test_param = "Test Param";
    RedBlackTree *test_tree = rbtree_init();
    assert_int_equal(rbtree_insert(&test_tree, test_param, "test_node"), SF_INT_RET_CODE_SUCCESS);
    assert_int_equal(rbtree_insert(&test_tree, test_param, "test_node"), SF_INT_RET_CODE_DUPLICATES);
    assert_int_equal(rbtree_insert(&test_tree, test_param, NULL), SF_INT_RET_CODE_ERROR);
    assert_int_equal(rbtree_insert(NULL, test_param, "test_node"), SF_INT_RET_CODE_ERROR);
    assert_int_equal(rbtree_insert(&test_tree, NULL, "test_node"), SF_INT_RET_CODE_ERROR);
    rbtree_deallocate(test_tree);
}

void test_multi_insert()
{
    int i = 0;
    char **test_param= NULL;
    char **test_node = NULL;
    char *temp_node = NULL;
    RedBlackTree *test_tree = rbtree_init();
    test_param = (char **)SF_CALLOC(10000, sizeof(char *));
    test_node = (char **)SF_CALLOC(10000,sizeof(char *));

    for (i = 0; i < 10000; i++)
    {
        test_param[i] = (char *)SF_CALLOC(1,sizeof("TEST_PARAM")+sizeof(i));
        test_node[i] = (char *)SF_CALLOC(1,sizeof("TEST_NODE")+sizeof(i));
        sprintf(test_param[i],"%s%d","TEST_PARAM",i);
        sprintf(test_node[i],"%s%d","TEST_NODE",i);
        assert_int_equal(rbtree_insert(&test_tree,test_param[i],test_node[i]),
                SF_INT_RET_CODE_SUCCESS);
    }
    for (i = 0; i < 10000; i++)
    {
        temp_node = (char *)rbtree_search_node(test_tree, test_node[i]);
        assert_non_null(temp_node);
        assert_string_equal(temp_node,test_param[i]);
    }
    for (i = 0; i < 10000; i++)
    {
        SF_FREE(test_param[i]);
        SF_FREE(test_node[i]);
    }
    SF_FREE(test_param);
    SF_FREE(test_node);
    rbtree_deallocate(test_tree);
}

void test_search()
{
    char * test_param = "Test Param";
    RedBlackTree *test_tree = rbtree_init();
    char *test_node = NULL;
    rbtree_insert(&test_tree, test_param, "test_node");
    test_node = (char *)rbtree_search_node(test_tree,"test_node");
    assert_non_null(test_node);
    assert_string_equal(test_node, test_param);
    assert_null(rbtree_search_node(test_tree, "absent_node"));
    assert_null(rbtree_search_node(NULL, "absent_node"));
    assert_null(rbtree_search_node(test_tree, NULL));
    rbtree_deallocate(test_tree);
}

int main (void) {
    const struct CMUnitTest tests[] =
            {
                cmocka_unit_test(test_init),
                cmocka_unit_test(test_insert),
                cmocka_unit_test(test_multi_insert),
                cmocka_unit_test(test_search)
            };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
}