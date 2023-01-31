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

export SNAPPY_VERSION="1.1.7"
curl -sL "https://github.com/google/snappy/archive/${SNAPPY_VERSION}.tar.gz" -o snappy-${SNAPPY_VERSION}.tar.gz
tar xf snappy-${SNAPPY_VERSION}.tar.gz
pushd snappy-${SNAPPY_VERSION}
CXXFLAGS='-DNDEBUG -O2' cmake -GNinja \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
    -DBUILD_SHARED_LIBS=OFF \
    -DSNAPPY_BUILD_TESTS=OFF \
    .
ninja install
popd
rm -rf snappy-${SNAPPY_VERSION}.tar.gz snappy-${SNAPPY_VERSION}
