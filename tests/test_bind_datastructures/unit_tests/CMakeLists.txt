project(test_bind_ds)

if (UNIX)
    find_library(CMOCKA_LIB libcmocka.a PATHS ../deps-build/${PLATFORM}/cmocka/lib/ REQUIRED)
else()
    find_library(CMOCKA_LIB libcmocka.a PATHS ../deps-build/${PLATFORM}/${VSDIR}/cmocka/lib/ REQUIRED)
endif ()

add_executable(test_unit_rbtree test_unit_rbtree.c)
add_executable(test_unit_treemap test_unit_treemap.c)

if(UNIX)
    target_include_directories(test_unit_rbtree PUBLIC ../../../deps-build/${PLATFORM}/cmocka/include)
    target_include_directories(test_unit_treemap PUBLIC ../../../deps-build/${PLATFORM}/cmocka/include)
else()
    target_include_directories(test_unit_rbtree PUBLIC ../../../deps-build/${PLATFORM}/${VSDIR}/cmocka/include)
    target_include_directories(test_unit_treemap PUBLIC ../../../deps-build/${PLATFORM}/${VSDIR}/cmocka/include)
endif()

target_link_libraries(test_unit_rbtree ${TESTLIB_OPTS_C})
target_link_libraries(test_unit_treemap ${TESTLIB_OPTS_C})

add_test(test_unit_rbtree test_unit_rbtree)
add_test(test_unit_treemap test_unit_treemap)
