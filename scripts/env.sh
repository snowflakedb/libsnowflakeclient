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

echo "==> Test Connection Parameters"
env | grep SNOWFLAKE | grep -v PASSWORD
