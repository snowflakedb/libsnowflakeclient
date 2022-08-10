#!/bin/sh
#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License").
# You may not use this file except in compliance with the License.
# A copy of the License is located at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# or in the "license" file accompanying this file. This file is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
# express or implied. See the License for the specific language governing
# permissions and limitations under the License.

LITANI=${LITANI:-litani}

if [ "$1" != "--no-standalone" ]; then
  "${LITANI}" init --project-name "front-page-output examples"
fi

"${LITANI}" add-job \
    --pipeline-name "fibonacci" \
    --description "write fibonacci to csv" \
    --command "python3 scripts/fib.py -o fib.csv" \
    --ci-stage test \
    --outputs "fib.csv"

"${LITANI}" add-job \
    --pipeline-name "fibonacci" \
    --description "plot fibonacci results" \
    --command "gnuplot scripts/fib.plt" \
    --ci-stage report \
    --inputs "fib.csv" \
    --tags "front-page-text"

"${LITANI}" add-job \
    --pipeline-name "fibonacci" \
    --description "table fibonacci results" \
    --command "python3 scripts/fib-table.py -i fib.csv" \
    --ci-stage report \
    --inputs "fib.csv" \
    --tags "front-page-text"

if [ "$1" != "--no-standalone" ]; then
  "${LITANI}" run-build
fi
