cmake_minimum_required(VERSION 3.17)
include(FindPackageHandleStandardArgs)
include(${CMAKE_CURRENT_LIST_DIR}/Utils.cmake)

find_library(TELEMETRY_LIB libtelemetry.a cmocka_a PATHS ${DEPS_DIR}/oob/lib/ NO_DEFAULT_PATH)
find_path(TELEMETRY_INCLUDE_DIR oobtelemetry.h PATHS ${DEPS_DIR}/oob/include/ NO_DEFAULT_PATH)
find_path(CURL_INCLUDE_DIR curl/curl.h PATHS ${DEPS_DIR}/curl/include/ NO_DEFAULT_PATH)

find_package_handle_standard_args(Telemetry REQUIRED_VARS TELEMETRY_LIB TELEMETRY_INCLUDE_DIR CURL_INCLUDE_DIR)

if (Telemetry_FOUND)
    mark_as_advanced(TELEMETRY_LIB)
    mark_as_advanced(TELEMETRY_INCLUDE_DIR)
endif()

if (Telemetry_FOUND AND NOT TARGET Telemetry::Telemetry)
    add_library(Telemetry::Telemetry STATIC IMPORTED)
    set_property(TARGET Telemetry::Telemetry PROPERTY IMPORTED_LOCATION ${TELEMETRY_LIB})
    target_include_directories(Telemetry::Telemetry INTERFACE ${TELEMETRY_INCLUDE_DIR})
    target_include_directories(Telemetry::Telemetry INTERFACE ${CURL_INCLUDE_DIR})
endif ()
