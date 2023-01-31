#!/bin/bash

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

# This script requires Python 3 and clang-format, which should already be
# on your system. See r/README.md for further guidance

set -e

SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CPP_BUILD_SUPPORT=$SOURCE_DIR/../cpp/build-support

# Run clang-format
: ${CLANG_FORMAT:=$(. "${SOURCE_DIR}/../.env" && echo clang-format-${CLANG_TOOLS})}
$CPP_BUILD_SUPPORT/run_clang_format.py \
    --clang_format_binary=$CLANG_FORMAT \
    --exclude_glob=$CPP_BUILD_SUPPORT/lint_exclusions.txt \
    --source_dir=$SOURCE_DIR/src --quiet $1


# Run cpplint
CPPLINT=$CPP_BUILD_SUPPORT/cpplint.py
$CPP_BUILD_SUPPORT/run_cpplint.py \
    --cpplint_binary=$CPPLINT \
    --exclude_glob=$CPP_BUILD_SUPPORT/lint_exclusions.txt \
    --source_dir=$SOURCE_DIR/src --quiet
