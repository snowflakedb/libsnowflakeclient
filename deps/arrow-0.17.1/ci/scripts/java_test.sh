#!/usr/bin/env bash
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

set -ex

arrow_dir=${1}
source_dir=${1}/java
cpp_build_dir=${2}/cpp/${ARROW_BUILD_TYPE:-debug}

# for jni and plasma tests
export LD_LIBRARY_PATH=${ARROW_HOME}/lib:${LD_LIBRARY_PATH}
export PLASMA_STORE=${ARROW_HOME}/bin/plasma-store-server

mvn="mvn -B -Dorg.slf4j.simpleLogger.log.org.apache.maven.cli.transfer.Slf4jMavenTransferListener=warn"

pushd ${source_dir}

${mvn} test

if [ "${ARROW_GANDIVA_JAVA}" = "ON" ]; then
  ${mvn} test -Parrow-jni -pl adapter/orc,gandiva -Darrow.cpp.build.dir=${cpp_build_dir}
fi

if [ "${ARROW_PLASMA}" = "ON" ]; then
  pushd ${source_dir}/plasma
  java -cp target/test-classes:target/classes \
       -Djava.library.path=${cpp_build_dir} \
       org.apache.arrow.plasma.PlasmaClientTest
  popd
fi

popd
