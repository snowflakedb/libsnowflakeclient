#!/usr/bin/env python3
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


import os
import pathlib
import re
import subprocess


litani = os.getenv("LITANI", "litani")
this_dir = pathlib.Path(__file__).parent

subprocess.run(
    [litani, "init", "--project-name", "rich-output-examples"], check=True)

pat = re.compile(r"run-\d.sh")
for fyle in this_dir.iterdir():
    if pat.match(fyle.name):
        subprocess.run([fyle, "--no-standalone"], check=True)

subprocess.run([litani, "run-build"], check=True)
