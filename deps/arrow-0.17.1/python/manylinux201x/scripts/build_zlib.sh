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

curl -sL https://zlib.net/zlib-1.2.11.tar.gz -o /zlib-1.2.11.tar.gz
tar xf zlib-1.2.11.tar.gz
pushd zlib-1.2.11
CFLAGS=-fPIC ./configure --static
make -j8
make install
popd
rm -rf zlib-1.2.11.tar.gz zlib-1.2.11
