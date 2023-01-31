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

export THRIFT_VERSION=0.12.0
wget https://archive.apache.org/dist/thrift/${THRIFT_VERSION}/thrift-${THRIFT_VERSION}.tar.gz
tar xf thrift-${THRIFT_VERSION}.tar.gz
pushd thrift-${THRIFT_VERSION}
mkdir build-tmp
pushd build-tmp
cmake -DCMAKE_BUILD_TYPE=release \
    -DCMAKE_CXX_FLAGS=-fPIC \
    -DCMAKE_C_FLAGS=-fPIC \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_INSTALL_RPATH=/usr/local/lib \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DWITH_QT4=OFF \
    -DWITH_C_GLIB=OFF \
    -DWITH_JAVA=OFF \
    -DWITH_PYTHON=OFF \
    -DWITH_HASKELL=OFF \
    -DWITH_CPP=ON \
    -DWITH_STATIC_LIB=ON \
    -DWITH_SHARED_LIB=OFF \
    -DBoost_NAMESPACE=arrow_boost \
    -DBOOST_ROOT=/arrow_boost_dist \
    -GNinja ..
ninja install
popd
popd
rm -rf thrift-${THRIFT_VERSION}.tar.gz thrift-${THRIFT_VERSION}
