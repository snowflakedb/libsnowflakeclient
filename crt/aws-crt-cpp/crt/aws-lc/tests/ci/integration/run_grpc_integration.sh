#!/bin/bash -exu
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0 OR ISC

source tests/ci/common_posix_setup.sh

# SYS_ROOT
#  |
#  - SRC_ROOT(aws-lc)
#  |
#  - SCRATCH_FOLDER
#    |
#    - grpc
#    - AWS_LC_BUILD_FOLDER
#    - AWS_LC_INSTALL_FOLDER
#    - GRPC_BUILD_FOLDER

# Assumes script is executed from the root of aws-lc directory
SCRATCH_FOLDER="${SYS_ROOT}/GRPC_BUILD_ROOT"
GRPC_SRC_FOLDER="${SCRATCH_FOLDER}/grpc"
GRPC_BUILD_FOLDER="${SCRATCH_FOLDER}/grpc/cmake/build"
AWS_LC_BUILD_FOLDER="${SCRATCH_FOLDER}/aws-lc-build"
AWS_LC_INSTALL_FOLDER="${SCRATCH_FOLDER}/aws-lc-install"

mkdir -p ${SCRATCH_FOLDER}
rm -rf ${SCRATCH_FOLDER}/*
cd ${SCRATCH_FOLDER}
mkdir -p ${AWS_LC_BUILD_FOLDER} ${AWS_LC_INSTALL_FOLDER}

git clone --depth 1 https://github.com/grpc/grpc.git ${GRPC_SRC_FOLDER}
cd ${GRPC_SRC_FOLDER}
git submodule update --recursive --init

aws_lc_build ${SRC_ROOT} ${AWS_LC_BUILD_FOLDER} ${AWS_LC_INSTALL_FOLDER} -DBUILD_SHARED_LIBS=1

mkdir -p "${GRPC_SRC_FOLDER}/cmake/build"
cd "${GRPC_SRC_FOLDER}/cmake/build"
cmake -GNinja -DgRPC_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release  -DgRPC_SSL_PROVIDER=package  -DBUILD_SHARED_LIBS=ON  -DOPENSSL_ROOT_DIR="${AWS_LC_INSTALL_FOLDER}" ../..
ninja

# grpc tests expect to use relative paths to certificates and test files
cd "${GRPC_SRC_FOLDER}"
python3 tools/run_tests/start_port_server.py
for file in cmake/build/*; do
    if [[ -x "$file" && ( "$file" == *ssl* || "$file" == *tls* || "$file" == *cert* ) ]]; then
        ./"$file"
    fi
done

ldd "${GRPC_SRC_FOLDER}/cmake/build/libgrpc.so" | grep "${AWS_LC_INSTALL_FOLDER}/lib/libcrypto.so" || exit 1
ldd "${GRPC_SRC_FOLDER}/cmake/build/libgrpc.so" | grep "${AWS_LC_INSTALL_FOLDER}/lib/libssl.so" || exit 1