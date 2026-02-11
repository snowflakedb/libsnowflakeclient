#!/bin/bash -e

set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export WORKSPACE=${WORKSPACE:-/tmp}
export GIT_COMMIT=${client_git_commit:-$(git rev-parse HEAD)}

gpg --quiet --batch --yes --decrypt --passphrase="$PARAMETERS_SECRET" --output $THIS_DIR/../.github/workflows/parameters_aws_auth_tests.json "$THIS_DIR/../.github/workflows/parameters_aws_auth_tests.json.gpg"

docker run \
  -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
  -v $WORKSPACE:/mnt/workspace \
  --rm \
  -e AWS_ACCESS_KEY_ID \
  -e AWS_SECRET_ACCESS_KEY \
  -e GIT_BRANCH \
  -e GIT_COMMIT \
  artifactory.ci1.us-west-2.aws-dev.app.snowflake.com/internal-production-docker-snowflake-virtual/docker/snowdrivers-test-external-browser-odbc:6 \
  "/mnt/host/ci/test/test_authentication.sh"
