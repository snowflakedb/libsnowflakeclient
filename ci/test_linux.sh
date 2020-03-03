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

docker pull "${TEST_IMAGE_NAME}"

docker run \
        -t \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e BUILD_TYPE=Debug \
        -e JOB_NAME \
        -e BUILD_NUMBER \
        -e LOCAL_USER_ID=$(id -u $USER) \
        -e PARAMETERS_SECRET \
        -e AWS_ACCESS_KEY_ID \
        -e AWS_SECRET_ACCESS_KEY \
        -w /mnt/host \
        "${BUILD_IMAGE_NAME}" \
        "ci/test/test.sh"
