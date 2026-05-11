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
        return 'JENKINS_{0}_{1}'.format(job_name, build_number).replace('-', '_')

    print("[WARN] The environment variable Jenkins (JOB_NAME or BUILD_NUMBER), RUNNER_TRACKING_ID is not set. No test schema will be created.")
    sys.exit(1)


def _load_private_key_der(key_path, key_pwd):
    """Load a PEM/DER private key file and return DER-encoded PKCS#8 bytes
    suitable for passing to snowflake.connector.connect(private_key=...)."""
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.serialization import load_pem_private_key
    from cryptography.hazmat.backends import default_backend

    with open(key_path, 'rb') as key_file:
        pem_data = key_file.read()

    pwd_bytes = key_pwd.encode('utf-8') if key_pwd else None
    private_key_obj = load_pem_private_key(
        pem_data, password=pwd_bytes, backend=default_backend()
    )
    return private_key_obj.private_bytes(
        encoding=serialization.Encoding.DER,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )


def init_connection_params():
    params = {
        'account': os.getenv("SNOWFLAKE_TEST_ACCOUNT"),
        'user': os.getenv("SNOWFLAKE_TEST_USER"),
        'database': os.getenv("SNOWFLAKE_TEST_DATABASE"),
        'role': os.getenv("SNOWFLAKE_TEST_ROLE"),
    }

    private_key_file = os.getenv("SNOWFLAKE_TEST_PRIVATE_KEY_FILE")
    if private_key_file:
        if not os.path.isabs(private_key_file):
            workspace = os.getenv("WORKSPACE") or os.getcwd()
            private_key_file = os.path.join(workspace, private_key_file)
        try:
            params['private_key'] = _load_private_key_der(
                private_key_file, os.getenv("SNOWFLAKE_TEST_PRIVATE_KEY_PWD")
            )
            params['authenticator'] = 'SNOWFLAKE_JWT'
        except Exception as exc:
            print("ERROR: Failed to read private key file {0}: {1}".format(
                private_key_file, exc))
            sys.exit(1)
    else:
        params['password'] = os.getenv("SNOWFLAKE_TEST_PASSWORD")

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
