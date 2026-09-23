# Install script for directory: C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow/util" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/util/config.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/debug/Debug/arrow_static.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/debug/Release/arrow_static.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/debug/MinSizeRel/arrow_static.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/debug/RelWithDebInfo/arrow_static.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/ArrowConfig.cmake"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/ArrowConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow/ArrowTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow/ArrowTargets.cmake"
         "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow/ArrowTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow/ArrowTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets-debug.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets-minsizerel.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets-relwithdebinfo.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/Debug/arrow.pc")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/Release/arrow.pc")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/MinSizeRel/arrow.pc")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/RelWithDebInfo/arrow.pc")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow" TYPE FILE FILES
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/api.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/array.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/buffer.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/buffer_builder.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/builder.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/chunk_resolver.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/chunked_array.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/compare.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/config.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/datum.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/device.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/device_allocation_type_set.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/extension_type.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/memory_pool.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/memory_pool_test.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/pretty_print.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/record_batch.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/result.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/scalar.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/sparse_tensor.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/status.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/stl.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/stl_allocator.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/stl_iterator.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/table.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/table_builder.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/tensor.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/type.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/type_fwd.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/type_traits.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/visit_array_inline.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/visit_data_inline.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/visit_scalar_inline.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/visit_type_inline.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/visitor.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/visitor_generate.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/ArrowOptions.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/arrow-config.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/testing/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/array/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/compute/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/extension/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/io/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/tensor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/util/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/vendored/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/ipc/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
