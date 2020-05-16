#!/usr/bin/env python
#
# Snowflake test utils
#
import os
import sys

def get_test_schema():
    # GITHUB_ACTIONS
    github_sha = os.getenv('GITHUB_SHA')
    github_runner_id = os.getenv('RUNNER_TRACKING_ID')
    if github_sha and github_runner_id:
        return github_runner_id.replace('-', '_') + '_' + github_sha

    # Jenkins
    build_number = os.getenv('BUILD_NUMBER')
    job_name = os.getenv('JOB_NAME')
    if build_number and job_name:
        return 'JENKINS_{0}_{1}'.format(job_name, build_number).replace('-','_')
    
    print("[WARN] The environment variable Jenkins (JOB_NAME or BUILD_NUMBER), RUNNER_TRACKING_ID is not set. No test schema will be created.")
    sys.exit(1)

def init_connection_params():
    params = {
        'account': os.getenv("SNOWFLAKE_TEST_ACCOUNT"),
        'user': os.getenv("SNOWFLAKE_TEST_USER"),
        'password': os.getenv("SNOWFLAKE_TEST_PASSWORD"),
        'database': os.getenv("SNOWFLAKE_TEST_DATABASE"),
        'role': os.getenv("SNOWFLAKE_TEST_ROLE"),
    }
    host = os.getenv("SNOWFLAKE_TEST_HOST")
    if host:
        params['host'] = host
    port = os.getenv("SNOWFLAKE_TEST_PORT")
    if port:
        params['port'] = port
    protocol = os.getenv("SNOWFLAKE_TEST_PROTOCOL")
    if protocol:
        params['protocol'] = protocol
    warehouse = os.getenv("SNOWFLAKE_TEST_WAREHOUSE")
    if warehouse:
        params['warehouse'] = warehouse

    return params
