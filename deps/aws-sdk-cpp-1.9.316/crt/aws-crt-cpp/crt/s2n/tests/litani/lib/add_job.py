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
# permissions and limitations under the License

import json
import logging
import sys
import uuid

from lib import litani


async def add_job(job_dict):
    cache_file = litani.get_cache_dir() / litani.CACHE_FILE
    with open(cache_file) as handle:
        cache_contents = json.load(handle)
    if job_dict["ci_stage"] not in cache_contents["stages"]:
        valid_stages = "', '".join(cache_contents["stages"])
        logging.error(
            "Invalid stage name '%s' was provided, possible "
            "stage names are: '%s'", job_dict["ci_stage"], valid_stages)
        sys.exit(1)

    jobs_dir = litani.get_cache_dir() / litani.JOBS_DIR
    jobs_dir.mkdir(exist_ok=True, parents=True)

    if job_dict["phony_outputs"]:
        if not job_dict["outputs"]:
            job_dict["outputs"] = job_dict["phony_outputs"]
        else:
            for phony_output in job_dict["phony_outputs"]:
                if phony_output not in job_dict["outputs"]:
                    job_dict["outputs"].append(phony_output)

    if "func" in job_dict:
        job_dict.pop("func")

    job_id = str(uuid.uuid4())
    job_dict["job_id"] = job_id
    job_dict["status_file"] = str(
        litani.get_status_dir() / ("%s.json" % job_id))

    logging.debug("Adding job: %s", json.dumps(job_dict, indent=2))

    with litani.atomic_write(jobs_dir / ("%s.json" % job_id)) as handle:
        print(json.dumps(job_dict, indent=2), file=handle)
