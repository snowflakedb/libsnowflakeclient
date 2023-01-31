#!/bin/bash
#
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
#

# Run this from cpp/ directory. flatc is expected to be in your path

CWD="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
SOURCE_DIR=$CWD/../src
FORMAT_DIR=$CWD/../../format

flatc -c -o $SOURCE_DIR/generated \
      --scoped-enums \
      $FORMAT_DIR/Message.fbs \
      $FORMAT_DIR/File.fbs \
      $FORMAT_DIR/Schema.fbs \
      $FORMAT_DIR/Tensor.fbs \
      $FORMAT_DIR/SparseTensor.fbs \
      src/arrow/ipc/feather.fbs

flatc -c -o $SOURCE_DIR/plasma \
      --gen-object-api \
      --scoped-enums \
      $SOURCE_DIR/plasma/common.fbs \
      $SOURCE_DIR/plasma/plasma.fbs
