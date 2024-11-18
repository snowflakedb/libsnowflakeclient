cmake_minimum_required(VERSION 3.17)
include(FindPackageHandleStandardArgs)
include(${CMAKE_CURRENT_LIST_DIR}/Utils.cmake)

find_library(CURL_LIB libcurl.a cmocka_a PATHS ${DEPS_DIR}/curl/lib/ NO_DEFAULT_PATH)
find_path(CURL_INCLUDE_DIR curl/curl.h PATHS ${DEPS_DIR}/curl/include/ NO_DEFAULT_PATH)

find_package_handle_standard_args(Curl REQUIRED_VARS CURL_LIB CURL_INCLUDE_DIR)

if (Curl_FOUND)
    mark_as_advanced(CURL_LIB)
    mark_as_advanced(CURL_INCLUDE_DIR)
endif()

if (Curl_FOUND AND NOT TARGET Curl::Curl)
    add_library(Curl::Curl STATIC IMPORTED)
    set_property(TARGET Curl::Curl PROPERTY IMPORTED_LOCATION ${CURL_LIB})
    target_link_libraries(Curl::Curl INTERFACE Telemetry::Telemetry)
    target_include_directories(Curl::Curl INTERFACE ${CURL_INCLUDE_DIR})
endif ()
