#!/bin/bash -e

set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export WORKSPACE=${WORKSPACE:-/tmp}
export INTERNAL_REPO=nexus.int.snowflakecomputing.com:8086
export GIT_COMMIT=${client_git_commit:-$(git rev-parse HEAD)}

source $THIS_DIR/scripts/login_internal_docker.sh
gpg --quiet --batch --yes --decrypt --passphrase="$PARAMETERS_SECRET" --output $THIS_DIR/../.github/workflows/parameters_aws_auth_tests.json "$THIS_DIR/../.github/workflows/parameters_aws_auth_tests.json.gpg"

docker run \
  -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
  -v $WORKSPACE:/mnt/workspace \
  --rm \
  -e AWS_ACCESS_KEY_ID \
  -e AWS_SECRET_ACCESS_KEY \
  -e GIT_BRANCH \
  -e GIT_COMMIT \
  nexus.int.snowflakecomputing.com:8086/docker/snowdrivers-test-external-browser-odbc:5 \
  "/mnt/host/ci/test/test_authentication.sh"
