cmake_minimum_required(VERSION 3.17)
include(FindPackageHandleStandardArgs)
include(${CMAKE_CURRENT_LIST_DIR}/Utils.cmake)

find_library(BOOST_ATOMIC_LIB boost_atomic PATHS ${DEPS_DIR}/boost/lib/ NO_DEFAULT_PATH)
find_library(BOOST_REGEX_LIB boost_regex PATHS ${DEPS_DIR}/boost/lib/ NO_DEFAULT_PATH)
find_library(BOOST_SYSTEM_LIB boost_system PATHS ${DEPS_DIR}/boost/lib/ NO_DEFAULT_PATH)
find_library(BOOST_FILESYSTEM_LIB boost_filesystem PATHS ${DEPS_DIR}/boost/lib/ NO_DEFAULT_PATH)
find_path(BOOST_INCLUDE_DIR boost/version.hpp PATHS ${DEPS_DIR}/boost/include/ NO_DEFAULT_PATH)

find_package_handle_standard_args(Boost REQUIRED_VARS BOOST_ATOMIC_LIB BOOST_FILESYSTEM_LIB BOOST_REGEX_LIB BOOST_SYSTEM_LIB BOOST_INCLUDE_DIR)

if (Boost_FOUND)
    mark_as_advanced(BOOST_ATOMIC_LIB)
    mark_as_advanced(BOOST_REGEX_LIB)
    mark_as_advanced(BOOST_SYSTEM_LIB)
    mark_as_advanced(BOOST_FILESYSTEM_LIB)
    mark_as_advanced(BOOST_INCLUDE_DIR)
endif()

if (Boost_FOUND AND NOT TARGET Boost::Boost)
    add_library(Boost::Boost STATIC IMPORTED)
    target_link_libraries(
        Boost::Boost
            INTERFACE ${BOOST_ATOMIC_LIB}
            INTERFACE ${BOOST_REGEX_LIB}
            INTERFACE ${BOOST_SYSTEM_LIB}
            INTERFACE ${BOOST_FILESYSTEM_LIB}
    )
    target_include_directories(Boost::Boost INTERFACE ${BOOST_INCLUDE_DIR})
endif ()
