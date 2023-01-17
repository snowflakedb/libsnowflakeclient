#!/bin/bash -ex
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

export BROTLI_VERSION="1.0.7"
curl -sL "https://github.com/google/brotli/archive/v${BROTLI_VERSION}.tar.gz" -o brotli-${BROTLI_VERSION}.tar.gz
tar xf brotli-${BROTLI_VERSION}.tar.gz
pushd brotli-${BROTLI_VERSION}
mkdir build
pushd build
cmake -DCMAKE_BUILD_TYPE=release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -GNinja \
    ..
ninja install
popd
popd
rm -rf brotli-${BROTLI_VERSION}.tar.gz brotli-${BROTLI_VERSION}
