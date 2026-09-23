# Install script for directory: C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow/util" TYPE FILE FILES
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/algorithm.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/align_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/aligned_storage.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/async_generator.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/async_generator_fwd.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/async_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/base64.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/basic_decimal.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/benchmark_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/binary_view_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bit_block_counter.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bit_run_reader.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bit_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bitmap.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bitmap_builders.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bitmap_generate.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bitmap_ops.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bitmap_reader.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bitmap_visit.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/bitmap_writer.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/byte_size.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/cancel.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/checked_cast.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/compare.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/compression.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/concurrent_map.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/converter.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/cpu_info.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/crc32.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/debug.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/decimal.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/delimiting.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/endian.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/float16.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/formatting.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/functional.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/future.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/hash_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/hashing.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/int_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/int_util_overflow.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/io_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/iterator.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/key_value_metadata.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/launder.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/list_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/logger.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/logging.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/macros.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/math_constants.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/mutex.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/parallel.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/pcg_random.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/prefetch.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/queue.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/range.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/ree_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/regex.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/rows_to_batches.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/secure_string.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/simd.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/small_vector.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/span.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/string.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/string_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/task_group.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/test_common.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/thread_pool.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/time.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/tracing.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/type_fwd.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/type_traits.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/ubsan.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/union_util.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/unreachable.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/uri.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/utf8.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/value_parsing.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/vector.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/visibility.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/windows_compatibility.h"
    "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/src/arrow/util/windows_fixup.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/workspaces/libsnowflakeclient/deps/arrow-22.0.0/cpp/cmake-build-win64-VS17-Debug/src/arrow/util/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
