#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

unset(GRPC_ALT_VERSION)
if(gRPC_ROOT)
  find_library(GRPC_GPR_LIB gpr
               PATHS ${gRPC_ROOT}
               PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
               NO_DEFAULT_PATH)
  find_library(GRPC_GRPC_LIB grpc
               PATHS ${gRPC_ROOT}
               PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
               NO_DEFAULT_PATH)
  find_library(GRPC_GRPCPP_LIB grpc++
               PATHS ${gRPC_ROOT}
               PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
               NO_DEFAULT_PATH)
  find_library(GRPC_ADDRESS_SORTING_LIB address_sorting
               PATHS ${gRPC_ROOT}
               PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
               NO_DEFAULT_PATH)
  find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin NO_DEFAULT_PATH
               PATHS ${gRPC_ROOT}
               PATH_SUFFIXES "bin")
  find_path(GRPC_INCLUDE_DIR
            NAMES grpc/grpc.h
            PATHS ${gRPC_ROOT}
            NO_DEFAULT_PATH
            PATH_SUFFIXES ${INCLUDE_PATH_SUFFIXES})
else()
  pkg_check_modules(GRPC_PC grpc++)
  if(GRPC_PC_FOUND)
    set(GRPC_ALT_VERSION "${GRPC_PC_VERSION}")
    set(GRPC_INCLUDE_DIR "${GRPC_PC_INCLUDEDIR}")
    list(APPEND GRPC_PC_LIBRARY_DIRS "${GRPC_PC_LIBDIR}")
    message(STATUS "${GRPC_PC_LIBRARY_DIRS}")

    find_library(GRPC_GPR_LIB gpr
                 PATHS ${GRPC_PC_LIBRARY_DIRS}
                 PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
                 NO_DEFAULT_PATH)
    find_library(GRPC_GRPC_LIB grpc
                 PATHS ${GRPC_PC_LIBRARY_DIRS}
                 PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
                 NO_DEFAULT_PATH)
    find_library(GRPC_GRPCPP_LIB grpc++
                 PATHS ${GRPC_PC_LIBRARY_DIRS}
                 PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
                 NO_DEFAULT_PATH)
    find_library(GRPC_ADDRESS_SORTING_LIB address_sorting
                 PATHS ${GRPC_PC_LIBRARY_DIRS}
                 PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
                 NO_DEFAULT_PATH)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin
                 HINTS ${GRPC_PC_PREFIX}
                 NO_DEFAULT_PATH
                 PATH_SUFFIXES "bin")
  else()
    find_library(GRPC_GPR_LIB gpr PATH_SUFFIXES ${LIB_PATH_SUFFIXES})
    find_library(GRPC_GRPC_LIB grpc PATH_SUFFIXES ${LIB_PATH_SUFFIXES})
    find_library(GRPC_GRPCPP_LIB grpc++ PATH_SUFFIXES ${LIB_PATH_SUFFIXES})
    find_library(GRPC_ADDRESS_SORTING_LIB address_sorting
                 PATH_SUFFIXES ${LIB_PATH_SUFFIXES})
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin PATH_SUFFIXES "bin")
    find_path(GRPC_INCLUDE_DIR NAMES grpc/grpc.h PATH_SUFFIXES ${INCLUDE_PATH_SUFFIXES})
  endif()
endif()

set(GRPC_ALT_FIND_PACKAGE_ARGS
    gRPCAlt
    REQUIRED_VARS
    GRPC_INCLUDE_DIR
    GRPC_GPR_LIB
    GRPC_GRPC_LIB
    GRPC_GRPCPP_LIB
    GRPC_CPP_PLUGIN)
if(GRPC_ALT_VERSION)
  list(APPEND GRPC_ALT_FIND_PACKAGE_ARGS VERSION_VAR GRPC_ALT_VERSION)
endif()
find_package_handle_standard_args(${GRPC_ALT_FIND_PACKAGE_ARGS})

if(gRPCAlt_FOUND)
  add_library(gRPC::gpr UNKNOWN IMPORTED)
  set_target_properties(gRPC::gpr
                        PROPERTIES IMPORTED_LOCATION "${GRPC_GPR_LIB}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${GRPC_INCLUDE_DIR}")

  add_library(gRPC::grpc UNKNOWN IMPORTED)
  set_target_properties(gRPC::grpc
                        PROPERTIES IMPORTED_LOCATION "${GRPC_GRPC_LIB}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${GRPC_INCLUDE_DIR}")

  add_library(gRPC::grpc++ UNKNOWN IMPORTED)
  set_target_properties(gRPC::grpc++
                        PROPERTIES IMPORTED_LOCATION "${GRPC_GRPCPP_LIB}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${GRPC_INCLUDE_DIR}")

  if(GRPC_ADDRESS_SORTING_LIB)
    # Address sorting is optional and not always required.
    add_library(gRPC::address_sorting UNKNOWN IMPORTED)
    set_target_properties(gRPC::address_sorting
                          PROPERTIES IMPORTED_LOCATION "${GRPC_ADDRESS_SORTING_LIB}"
                                     INTERFACE_INCLUDE_DIRECTORIES "${GRPC_INCLUDE_DIR}")
  endif()

  add_executable(gRPC::grpc_cpp_plugin IMPORTED)
  set_target_properties(gRPC::grpc_cpp_plugin
                        PROPERTIES IMPORTED_LOCATION ${GRPC_CPP_PLUGIN})
endif()
