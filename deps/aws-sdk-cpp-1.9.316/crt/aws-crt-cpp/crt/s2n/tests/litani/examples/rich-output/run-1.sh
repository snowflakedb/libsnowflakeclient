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
  "${LITANI}" init --project-name "literal-stdout-examples"
fi

"${LITANI}" add-job \
  --pipeline-name "verify assumptions" \
  --description "list out assumptions" \
  --command "cat assumptions.html" \
  --ci-stage test \
  --phony-outputs assumptions.html \
  --tags literal-stdout

"${LITANI}" add-job \
  --pipeline-name "conduct analysis" \
  --description "generate the histogram" \
  --command "gnuplot -c histogram.dat" \
  --ci-stage report \
  --inputs file.dat \
  --phony-outputs histogram.svg \
  --tags literal-stdout

if [ "$1" != "--no-standalone" ]; then
  "${LITANI}" run-build
fi
