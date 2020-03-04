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
TARGET_DOCKER_TEST_IMAGE=${TARGET_DOCKER_TEST_IMAGE:-$DRIVER_NAME-centos6-default}
TEST_IMAGE_NAME="${TEST_IMAGE_NAMES[$TARGET_DOCKER_TEST_IMAGE]}"
docker pull "${TEST_IMAGE_NAME}"
docker run \
        -t \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e BUILD_TYPE=$BUILD_TYPE \
        -e JOB_NAME \
        -e BUILD_NUMBER \
        -e LOCAL_USER_ID=$(id -u $USER) \
        -e PARAMETERS_SECRET \
        -e AWS_ACCESS_KEY_ID \
        -e AWS_SECRET_ACCESS_KEY \
        -e GITHUB_ACTIONS \
        -e GITHUB_SHA \
        -e RUNNER_TRACKING_ID \
        -w /mnt/host \
        "${TEST_IMAGE_NAME}" \
        "ci/test/test.sh"
