#!/bin/bash -e
#
# Build libsnowflakeclient
#
set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/_init.sh

export GIT_COMMIT=`git rev-parse HEAD`
libsnowflake_ver='0.4.5'

echo $THIS_DIR

docker pull "${BUILD_IMAGE_NAME}"

docker run \
        -t \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e BUILD_TYPE=Debug \
        -e LOCAL_USER_ID=$(id -u $USER) \
        -e AWS_ACCESS_KEY_ID \
        -e AWS_SECRET_ACCESS_KEY \
        -w /mnt/host \
        "${BUILD_IMAGE_NAME}" \
        "ci/build/build.sh"

