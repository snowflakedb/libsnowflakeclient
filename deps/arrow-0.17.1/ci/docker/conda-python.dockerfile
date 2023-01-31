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

ARG repo
ARG arch
FROM ${repo}:${arch}-conda-cpp

# install python specific packages
ARG python=3.6
COPY ci/conda_env_python.yml /arrow/ci/
RUN conda install -q \
        --file arrow/ci/conda_env_python.yml \
        $([ "$python" == "3.6" -o "$python" == "3.7" ] && echo "pickle5") \
        python=${python} \
        nomkl && \
    conda clean --all

ENV ARROW_PYTHON=ON \
    ARROW_BUILD_STATIC=OFF \
    ARROW_BUILD_TESTS=OFF \
    ARROW_BUILD_UTILITIES=OFF \
    ARROW_TENSORFLOW=ON \
    ARROW_USE_GLOG=OFF
