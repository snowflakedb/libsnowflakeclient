cmake_minimum_required(VERSION 3.17)
include(FindPackageHandleStandardArgs)
include(${CMAKE_CURRENT_LIST_DIR}/Utils.cmake)

find_library(OPENSSL_SSL_LIB ssl PATHS ${DEPS_DIR}/openssl/lib/ NO_DEFAULT_PATH)
find_library(OPENSSL_CRYPTO_LIB crypto PATHS ${DEPS_DIR}/openssl/lib/ NO_DEFAULT_PATH)
find_path(OPENSSL_INCLUDE_DIR openssl/core.h PATHS ${DEPS_DIR}/openssl/include/ NO_DEFAULT_PATH)

find_package_handle_standard_args(OpenSSL REQUIRED_VARS OPENSSL_SSL_LIB OPENSSL_CRYPTO_LIB OPENSSL_INCLUDE_DIR)

if (OpenSSL_FOUND)
    mark_as_advanced(OPENSSL_SSL_LIB)
    mark_as_advanced(OPENSSL_CRYPTO_LIB)
    mark_as_advanced(OPENSSL_INCLUDE_DIR)
endif()

if (OpenSSL_FOUND AND NOT TARGET OpenSSL::SSL)
    add_library(OpenSSL::SSL STATIC IMPORTED)
    set_property(TARGET OpenSSL::SSL PROPERTY IMPORTED_LOCATION ${OPENSSL_SSL_LIB})
    target_include_directories(OpenSSL::SSL INTERFACE ${OPENSSL_INCLUDE_DIR})
endif ()

if (OpenSSL_FOUND AND NOT TARGET OpenSSL::Crypto)
    add_library(OpenSSL::Crypto STATIC IMPORTED)
    set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIB})
    target_include_directories(OpenSSL::Crypto INTERFACE ${OPENSSL_INCLUDE_DIR})
endif ()
