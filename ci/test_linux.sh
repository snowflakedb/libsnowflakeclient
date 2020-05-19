#!/bin/bash -e
#
# Test libsnowflakeclient for Linux
#
# - TEST_IMAGE_NAME - the target Docker image key. It must be registered in _init.sh
#

set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/_init.sh
CI_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

[[ -z "$BUILD_TYPE" ]] && echo "Set BUILD_TYPE. [Debug, Release]" && exit 1

if [[ -z "$GITHUB_ACTIONS" ]]; then
    export GIT_URL=${GIT_URL:-https://github.com/snowflakedb/libsnowflakeclient.git}
    export GIT_BRANCH=${GIT_BRANCH:-origin/$(git rev-parse --abbrev-ref HEAD)}
    export GIT_COMMIT=${GIT_COMMIT:-$(git rev-parse HEAD)}
else
    export GIT_URL=https://github.com/${GITHUB_REPOSITORY}.git
    export GIT_BRANCH=origin/$(basename ${GITHUB_REF})
    export GIT_COMMIT=${GITHUB_SHA}
fi

TARGET_DOCKER_TEST_IMAGE=${TARGET_DOCKER_TEST_IMAGE:-$DRIVER_NAME-centos6-default}
TEST_IMAGE_NAME="${TEST_IMAGE_NAMES[$TARGET_DOCKER_TEST_IMAGE]}"
docker pull "${TEST_IMAGE_NAME}"
docker run \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e LOCAL_USER_ID=$(id -u $USER) \
        -e cloud_provider \
        -e SNOWFLAKE_TEST_CA_BUNDLE_FILE \
        -e GIT_COMMIT \
        -e GIT_BRANCH \
        -e GIT_URL \
        -e BUILD_TYPE \
        -e JOB_NAME \
        -e BUILD_NUMBER \
        -e PARAMETERS_SECRET \
        -e AWS_ACCESS_KEY_ID \
        -e AWS_SECRET_ACCESS_KEY \
        -e GITHUB_ACTIONS \
        -e GITHUB_SHA \
        -e RUNNER_TRACKING_ID \
        -w /mnt/host \
        "${TEST_IMAGE_NAME}" \
        "/mnt/host/ci/test/test.sh"
