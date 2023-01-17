#!/usr/bin/env bash
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

set -eux

target=$1

packages=()
case "${target}" in
  cpp|c_glib|ruby)
    # ccache may be broken on MinGW.
    # packages+=(ccache)
    packages+=(${MINGW_PACKAGE_PREFIX}-boost)
    packages+=(${MINGW_PACKAGE_PREFIX}-brotli)
    packages+=(${MINGW_PACKAGE_PREFIX}-cmake)
    packages+=(${MINGW_PACKAGE_PREFIX}-gcc)
    packages+=(${MINGW_PACKAGE_PREFIX}-gflags)
    packages+=(${MINGW_PACKAGE_PREFIX}-grpc)
    packages+=(${MINGW_PACKAGE_PREFIX}-gtest)
    packages+=(${MINGW_PACKAGE_PREFIX}-lz4)
    packages+=(${MINGW_PACKAGE_PREFIX}-protobuf)
    packages+=(${MINGW_PACKAGE_PREFIX}-python3-numpy)
    packages+=(${MINGW_PACKAGE_PREFIX}-rapidjson)
    packages+=(${MINGW_PACKAGE_PREFIX}-snappy)
    packages+=(${MINGW_PACKAGE_PREFIX}-thrift)
    packages+=(${MINGW_PACKAGE_PREFIX}-zlib)
    packages+=(${MINGW_PACKAGE_PREFIX}-zstd)
  ;;
esac

case "${target}" in
  c_glib|ruby)
    packages+=(${MINGW_PACKAGE_PREFIX}-gobject-introspection)
    packages+=(${MINGW_PACKAGE_PREFIX}-gtk-doc)
    packages+=(${MINGW_PACKAGE_PREFIX}-meson)
    ;;
esac

pacman \
  --noconfirm \
  --sync \
  "${packages[@]}"
