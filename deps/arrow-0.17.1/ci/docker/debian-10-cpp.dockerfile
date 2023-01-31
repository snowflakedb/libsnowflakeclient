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

ARG arch=amd64
FROM ${arch}/debian:10

ENV DEBIAN_FRONTEND noninteractive

RUN \
  echo "deb http://deb.debian.org/debian buster-backports main" > \
    /etc/apt/sources.list.d/backports.list

ARG llvm
RUN apt-get update -y -q && \
    apt-get install -y -q --no-install-recommends \
        autoconf \
        ca-certificates \
        ccache \
        clang-${llvm} \
        cmake \
        g++ \
        gcc \
        gdb \
        git \
        libbenchmark-dev \
        libboost-all-dev \
        libbrotli-dev \
        libbz2-dev \
        libc-ares-dev \
        libgflags-dev \
        libgmock-dev \
        libgoogle-glog-dev \
        libgtest-dev \
        liblz4-dev \
        libre2-dev \
        libsnappy-dev \
        libssl-dev \
        libthrift-dev \
        libzstd-dev \
        llvm-${llvm}-dev \
        make \
        ninja-build \
        pkg-config \
        protobuf-compiler \
        rapidjson-dev \
        tzdata \
        zlib1g-dev \
        wget && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

ENV ARROW_BUILD_TESTS=ON \
    ARROW_DEPENDENCY_SOURCE=SYSTEM \
    ARROW_DATASET=ON \
    ARROW_FLIGHT=ON \
    ARROW_GANDIVA=ON \
    ARROW_HOME=/usr/local \
    ARROW_ORC=ON \
    ARROW_PARQUET=ON \
    ARROW_PLASMA=ON \
    ARROW_USE_CCACHE=ON \
    ARROW_WITH_BROTLI=ON \
    ARROW_WITH_BZ2=ON \
    ARROW_WITH_LZ4=ON \
    ARROW_WITH_SNAPPY=ON \
    ARROW_WITH_ZLIB=ON \
    ARROW_WITH_ZSTD=ON \
    cares_SOURCE=BUNDLED \
    CC=gcc \
    CXX=g++ \
    gRPC_SOURCE=BUNDLED \
    ORC_SOURCE=BUNDLED \
    PATH=/usr/lib/ccache/:$PATH \
    Protobuf_SOURCE=BUNDLED
