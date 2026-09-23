# Install script for directory: C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow/io" TYPE FILE FILES
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/api.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/buffered.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/caching.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/compressed.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/concurrency.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/file.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/hdfs.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/interfaces.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/memory.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/mman.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/slow.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/stdio.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/test_common.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/transform.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/io/type_fwd.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/io/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
