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

SLOW = False

def get_init_args():
    return {
        "kwargs": {
            "project": "foo",
        }
    }


def get_jobs():
    return [{
        "kwargs": {
            "command": "echo foo",
            "ci-stage": "build",
            "pipeline": "foo",
        }
    }, {
        "kwargs": {
            "command": "echo bar",
            "ci-stage": "build",
            "pipeline": "foo",
        }
    }]


def transform_jobs(jobs):
    new_jobs = []
    for job in jobs:
        if job["command"] != "echo bar":
            new_jobs.append(job)
    return new_jobs


def get_run_build_args():
    return {}


def check_run(run):
    jobs = run["pipelines"][0]["ci_stages"][0]["jobs"]
    return all((len(jobs) == 1, jobs[0]["stdout"][0].strip() == "foo"))
