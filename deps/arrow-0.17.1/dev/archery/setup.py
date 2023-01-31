#!/usr/bin/env python
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

import sys
from setuptools import setup


if sys.version_info < (3, 5):
    sys.exit('Python < 3.5 is not supported')


setup(
    name='archery',
    version="0.1.0",
    description='Apache Arrow Developers Tools',
    url='http://github.com/apache/arrow',
    maintainer='Arrow Developers',
    maintainer_email='dev@arrow.apache.org',
    packages=['archery'],
    install_requires=['click', 'pygithub'],
    tests_require=['pytest', 'ruamel.yaml'],
    entry_points='''
        [console_scripts]
        archery=archery.cli:archery
    ''',
)
