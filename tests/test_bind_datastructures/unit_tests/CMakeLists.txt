project(test_bind_ds)

find_package(Cmocka REQUIRED)

add_executable(test_unit_rbtree test_unit_rbtree.c)
add_executable(test_unit_treemap test_unit_treemap.c)

target_link_libraries(test_unit_rbtree ${TESTLIB_OPTS_C})
target_link_libraries(test_unit_rbtree Cmocka::Cmocka)
target_link_libraries(test_unit_treemap ${TESTLIB_OPTS_C})
target_link_libraries(test_unit_treemap Cmocka::Cmocka)

add_test(test_unit_rbtree test_unit_rbtree)
add_test(test_unit_treemap test_unit_treemap)
