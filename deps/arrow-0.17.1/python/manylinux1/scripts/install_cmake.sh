#!/bin/bash -e
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

# pip tries to install 3.16.3 but there is no wheel for this version yet so it tries to 
# build from source and fails.
# Pinning to 3.13.3 to avoid building cmake from source
export CMAKE_VERSION=3.13.3
/opt/python/cp37-cp37m/bin/pip install cmake==${CMAKE_VERSION} ninja
ln -s /opt/python/cp37-cp37m/bin/cmake /usr/bin/cmake
ln -s /opt/python/cp37-cp37m/bin/ninja /usr/bin/ninja
strip /opt/_internal/cpython-3.*/lib/python3.7/site-packages/cmake/data/bin/*
