#!/bin/bash
set -exo pipefail
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0 OR ISC

source tests/ci/common_posix_setup.sh

echo "Testing AWS-LC in debug mode."
build_and_test

echo "Testing AWS-LC in release mode."
build_and_test -DCMAKE_BUILD_TYPE=Release

echo "Testing AWS-LC with Dilithium3 enabled."
build_and_test -DENABLE_DILITHIUM=ON

echo "Testing AWS-LC small compilation."
build_and_test -DOPENSSL_SMALL=1 -DCMAKE_BUILD_TYPE=Release

echo "Testing AWS-LC with libssl off."
build_and_test -DBUILD_LIBSSL=OFF -DCMAKE_BUILD_TYPE=Release

echo "Testing AWS-LC in no asm mode."
build_and_test -DOPENSSL_NO_ASM=1 -DCMAKE_BUILD_TYPE=Release

echo "Testing building shared lib."
build_and_test -DBUILD_SHARED_LIBS=1 -DCMAKE_BUILD_TYPE=Release

if [[ "${AWSLC_C99_TEST}" == "1" ]]; then
    echo "Testing the C99 compatability of AWS-LC headers."
    ./tests/coding_guidelines/c99_gcc_test.sh
fi

if [[ "${AWSLC_CODING_GUIDELINES_TEST}" == "1" ]]; then
  echo "Testing that AWS-LC is compliant with the coding guidelines."
  source ./tests/coding_guidelines/coding_guidelines_test.sh
fi

# Lightly verify that uncommon build options does not break the build. Fist
# define a list of typical build options to verify the special build option with
build_options_to_test=("" "-DBUILD_SHARED_LIBS=1" "-DCMAKE_BUILD_TYPE=Release" "-DBUILD_SHARED_LIBS=1 -DCMAKE_BUILD_TYPE=Release")

## Build option: MY_ASSEMBLER_IS_TOO_OLD_FOR_AVX
for build_option in "${build_options_to_test[@]}"; do
  run_build ${build_option} -DMY_ASSEMBLER_IS_TOO_OLD_FOR_AVX=ON
done

## Build option: MY_ASSEMBLER_IS_TOO_OLD_FOR_512AVX
for build_option in "${build_options_to_test[@]}"; do
  run_build ${build_option} -DMY_ASSEMBLER_IS_TOO_OLD_FOR_512AVX=ON
done
