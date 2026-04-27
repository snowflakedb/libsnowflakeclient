#!/bin/bash -e
#
# Set the environment variables for tests
#

set -o pipefail

SCRIPTS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PARAMETER_FILE=$( cd "$SCRIPTS_DIR/.." && pwd)/parameters.json

export SNOWFLAKE_TEST_CA_BUNDLE_FILE=$SCRIPTS_DIR/../cacert.pem

[[ ! -e "$PARAMETER_FILE" ]] &&  echo "The parameter file doesn't exist: $PARAMETER_FILE" && exit 1

eval $(jq -r '.testconnection | to_entries | map("export \(.key)=\(.value|tostring)")|.[]' $PARAMETER_FILE)

# Resolve a relative SNOWFLAKE_TEST_PRIVATE_KEY_FILE against the repo root so
# the JWT authenticator can find it regardless of the current working directory.
if [[ -n "$SNOWFLAKE_TEST_PRIVATE_KEY_FILE" ]]; then
    REPO_ROOT=$( cd "$SCRIPTS_DIR/.." && pwd)
    case "$SNOWFLAKE_TEST_PRIVATE_KEY_FILE" in
        /*) ;; # already absolute
        *) export SNOWFLAKE_TEST_PRIVATE_KEY_FILE="$REPO_ROOT/$SNOWFLAKE_TEST_PRIVATE_KEY_FILE" ;;
    esac
fi

# Auth selection (mirrors snowflake-odbc):
#   - If a usable private key file is present, use keypair (JWT) auth.
#   - Else if SNOWFLAKE_TEST_PASSWORD is set (e.g. injected by Jenkins via
#     withCredentials), unset SNOWFLAKE_TEST_PRIVATE_KEY_FILE so that
#     sf_test_utils.init_connection_params() falls back to password auth.
#   - Else leave both unset and let the test fail with a clear message.
if [[ -n "$SNOWFLAKE_TEST_PRIVATE_KEY_FILE" && ! -f "$SNOWFLAKE_TEST_PRIVATE_KEY_FILE" ]]; then
    if [[ -n "$SNOWFLAKE_TEST_PASSWORD" ]]; then
        echo "[INFO] No private key file at $SNOWFLAKE_TEST_PRIVATE_KEY_FILE; falling back to password auth."
        unset SNOWFLAKE_TEST_PRIVATE_KEY_FILE
    else
        echo "[ERROR] No authentication credentials found! Provide either a decryptable private key (PRIVATEKEY_CSP_KEY) or SNOWFLAKE_TEST_PASSWORD."
    fi
fi

echo "==> Test Connection Parameters"
env | grep SNOWFLAKE | grep -v -E "(PASSWORD|PWD|SECRET|TOKEN)"

echo "CLOUD_PROVIDER is set to $CLOUD_PROVIDER"