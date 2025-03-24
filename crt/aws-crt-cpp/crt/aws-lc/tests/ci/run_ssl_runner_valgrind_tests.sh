#!/bin/bash -ex
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0 OR ISC

source tests/ci/common_posix_setup.sh

echo "Testing AWS-LC in debug mode under Valgrind."
build_and_test_ssl_runner_valgrind
