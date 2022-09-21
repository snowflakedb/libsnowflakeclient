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


import logging
import uuid
import json
import os
import pathlib
import re
import subprocess
import sys


litani = os.getenv("LITANI", "litani")
this_dir = pathlib.Path(__file__).parent

subprocess.run(
    [litani, "init", "--project-name", "adding a root job"], check=True)

subprocess.run(
    [str(this_dir / "original-run.sh"), "--no-standalone"], check=True)


# ``````````````````````````````````````````````````````````````````````````````
# IMPORTANT! bufsize must be 0, otherwise the C stdlib will buffer the jobs that
# we send back to Litani's stdin
#
proc = subprocess.Popen(
    [litani, "-v", "transform-jobs"], stdin=subprocess.PIPE,
    stdout=subprocess.PIPE, text=True, bufsize=0)

jobs = json.loads(proc.stdout.read())

# A file name that the new root job will output, and which the old root nodes
# will depend on as an input. This is not a real file that the new root command
# will write, so we'll pass it to --phony-outputs rather than --outputs.
dependency_node = str(uuid.uuid4())

for job in jobs:
    if not job["inputs"]:
        job["inputs"] = [dependency_node]

print(json.dumps(jobs), file=proc.stdin)
proc.stdin.close()
proc.wait()

# Now, add the new root job. (It's also fine to do this before or during the
# transformation, but remember to skip the new job when iterating!)

subprocess.run([
    litani, "add-job",
    "--pipeline", "qux",
    "--ci-stage", "build",
    "--command", ">&2 echo Starting && sleep 2",
    "--phony-outputs", dependency_node,
], check=True)

subprocess.run([litani, "run-build"], check=True)
