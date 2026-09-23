# Install script for directory: C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/workspaces/libsnowflakeclient/deps-build/win64/vs17/Debug/arrow")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow/testing" TYPE FILE FILES
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/async_test_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/builder.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/executor_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/extension_type.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/fixed_width_test_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/future_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/generator.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/gtest_compat.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/gtest_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/matchers.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/math.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/process.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/random.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/uniform_real.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/testing/visibility.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/testing/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
