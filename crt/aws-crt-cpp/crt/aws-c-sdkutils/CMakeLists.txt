# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0.

cmake_minimum_required(VERSION 3.0)
project(aws-c-sdkutils LANGUAGES C VERSION 0.1.0)

if (POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW) # Enable LTO/IPO if available in the compiler, see AwsCFlags
endif()

if (DEFINED CMAKE_PREFIX_PATH)
    file(TO_CMAKE_PATH "${CMAKE_PREFIX_PATH}" CMAKE_PREFIX_PATH)
endif()

if (DEFINED CMAKE_INSTALL_PREFIX)
    file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" CMAKE_INSTALL_PREFIX)
endif()

if (UNIX AND NOT APPLE)
    include(GNUInstallDirs)
elseif(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib")
endif()

# This is required in order to append /lib/cmake to each element in CMAKE_PREFIX_PATH
set(AWS_MODULE_DIR "/${CMAKE_INSTALL_LIBDIR}/cmake")
string(REPLACE ";" "${AWS_MODULE_DIR};" AWS_MODULE_PATH "${CMAKE_PREFIX_PATH}${AWS_MODULE_DIR}")
# Append that generated list to the module search path
list(APPEND CMAKE_MODULE_PATH ${AWS_MODULE_PATH})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

include(AwsCFlags)
include(AwsCheckHeaders)
include(AwsSharedLibSetup)
include(AwsSanitizers)
include(AwsFindPackage)

file(GLOB AWS_SDKUTILS_HEADERS
        "include/aws/sdkutils/*.h"
)

file(GLOB AWS_SDKUTILS_PRIVATE_HEADERS
        "include/aws/sdkutils/private/*.h"
)

file(GLOB AWS_SDKUTILS_SRC
        "source/*.c"
)

if (WIN32)
    if (MSVC)
        source_group("Header Files\\aws\\sdkutils" FILES ${AWS_SDKUTILS_HEADERS})
        source_group("Source Files" FILES ${AWS_SDKUTILS_SRC})
    endif ()
endif()

file(GLOB SDKUTILS_HEADERS
        ${AWS_SDKUTILS_HEADERS}
)

file(GLOB SDKUTILS_SRC
        ${AWS_SDKUTILS_SRC}
)

add_library(${PROJECT_NAME} ${SDKUTILS_SRC})
aws_set_common_properties(${PROJECT_NAME} NO_WEXTRA)
aws_prepare_symbol_visibility_args(${PROJECT_NAME} "AWS_SDKUTILS")
aws_add_sanitizers(${PROJECT_NAME})

aws_use_package(aws-c-common)
target_link_libraries(${PROJECT_NAME} PUBLIC ${DEP_AWS_LIBS})

# Our ABI is not yet stable
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION 1.0.0)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
# When we install, the generated header will be at the INSTALL_INTERFACE:include location,
# but at build time we need to explicitly include this here
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIR}>)

aws_prepare_shared_lib_exports(${PROJECT_NAME})

configure_file("cmake/${PROJECT_NAME}-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
  @ONLY)

aws_check_headers(${PROJECT_NAME} ${AWS_SDKUTILS_HEADERS})
install(FILES ${AWS_SDKUTILS_HEADERS} DESTINATION "include/aws/sdkutils" COMPONENT Development)

if (BUILD_SHARED_LIBS)
   set (TARGET_DIR "shared")
else()
   set (TARGET_DIR "static")
endif()

install(EXPORT "${PROJECT_NAME}-targets"
    DESTINATION "${LIBRARY_DIRECTORY}/${PROJECT_NAME}/cmake/${TARGET_DIR}/"
    NAMESPACE AWS::
    COMPONENT Development)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
    DESTINATION "${LIBRARY_DIRECTORY}/${PROJECT_NAME}/cmake"
    COMPONENT Development)

install(FILES ${EXPORT_MODULES}
        DESTINATION "${LIBRARY_DIRECTORY}/${PROJECT_NAME}/cmake/modules"
        COMPONENT Development)

if (NOT CMAKE_CROSSCOMPILING)
    include(CTest)
    if (BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
