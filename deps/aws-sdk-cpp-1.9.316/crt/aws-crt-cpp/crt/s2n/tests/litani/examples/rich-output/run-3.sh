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
  "${LITANI}" init \
    --project-name "front-page-output and literal-stdout examples"
fi

"${LITANI}" add-job \
    --pipeline-name "sin" \
    --description "write sin to csv" \
    --command "python3 scripts/sin.py -o sin.csv" \
    --ci-stage test \
    --outputs "sin.csv"

"${LITANI}" add-job \
    --pipeline-name "sin" \
    --description "display sin results" \
    --command \
    "gnuplot scripts/sin.plt; python3 scripts/sin-output.py -i sin.csv" \
    --ci-stage report \
    --inputs "sin.csv" \
    --tags front-page-text literal-stdout

if [ "$1" != "--no-standalone" ]; then
  "${LITANI}" run-build
fi
