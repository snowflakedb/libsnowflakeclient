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

SLOW = True
EXPECTED_PIPELINE_NAMES = ["zeta", "0", "00", "01", "1", "Beta", "alpha",
    "beta", "beta1", "beta2", "betaa", "delta", "epsilon", "gamma"]


def get_init_args():
    return {
        "kwargs": {
            "project": "sorted_pipeline_names",
        }
    }


def get_jobs():
    jobs = []
    jobs.append({
        "kwargs": {
            "command": "/usr/bin/false",
            "ci-stage": "build",
            "pipeline": EXPECTED_PIPELINE_NAMES[0],
        }
    })
    for pipeline in EXPECTED_PIPELINE_NAMES[1:]:
        job = {
            "kwargs": {
                "command": "/usr/bin/true",
                "ci-stage": "build",
                "pipeline": pipeline,
            }
        }
        jobs.append(job)
    return jobs


def get_run_build_args():
    return {}


def check_run(run):
    return [p["name"] for p in run["pipelines"]] == EXPECTED_PIPELINE_NAMES
