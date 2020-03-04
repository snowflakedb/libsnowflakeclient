#!/bin/bash -e
#
# Build libsnowflakeclient
#
set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/_init.sh

[[ -z "$BUILD_TYPE" ]] && echo "Specify BUILD_TYPE. [Debug, Release]" && exit 1

BUILD_IMAGE_NAME="${BUILD_IMAGE_NAMES[$DRIVER_NAME-centos6-default]}"
echo $BUILD_IMAGE_NAME
docker pull "${BUILD_IMAGE_NAME}"
docker run \
        -t \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e BUILD_TYPE=$BUILD_TYPE \
        -e LOCAL_USER_ID=$(id -u $USER) \
        -e AWS_ACCESS_KEY_ID \
        -e AWS_SECRET_ACCESS_KEY \
        -e GITHUB_ACTIONS \
        -w /mnt/host \
        "${BUILD_IMAGE_NAME}" \
        "ci/build/build.sh"

