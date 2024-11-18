cmake_minimum_required(VERSION 3.17)
include(FindPackageHandleStandardArgs)
include(${CMAKE_CURRENT_LIST_DIR}/Utils.cmake)

find_library(CMOCKA_LIB libcmocka.a cmocka_a PATHS ${DEPS_DIR}/cmocka/lib/ NO_DEFAULT_PATH)
find_path(CMOCKA_INCLUDE_DIR cmocka.h PATHS ${DEPS_DIR}/cmocka/include/ NO_DEFAULT_PATH)

find_package_handle_standard_args(Cmocka REQUIRED_VARS CMOCKA_LIB CMOCKA_INCLUDE_DIR)

if (Cmocka_FOUND)
    mark_as_advanced(CMOCKA_LIB)
    mark_as_advanced(CMOCKA_INCLUDE_DIR)
endif()

if (Cmocka_FOUND AND NOT TARGET Cmocka::Cmocka)
    add_library(Cmocka::Cmocka STATIC IMPORTED)
    set_property(TARGET Cmocka::Cmocka PROPERTY IMPORTED_LOCATION ${CMOCKA_LIB})
    target_include_directories(Cmocka::Cmocka INTERFACE ${CMOCKA_INCLUDE_DIR})
endif ()
