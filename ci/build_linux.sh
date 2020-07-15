#!/bin/bash -e
#
# Build libsnowflakeclient
#
set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/_init.sh
source $THIS_DIR/scripts/login_internal_docker.sh

[[ -z "$BUILD_TYPE" ]] && echo "Specify BUILD_TYPE. [Debug, Release]" && exit 1

if [[ -z "$GITHUB_ACTIONS" ]]; then
    export GIT_URL=${GIT_URL:-https://github.com/snowflakedb/libsnowflakeclient.git}
    export GIT_BRANCH=${GIT_BRANCH:-origin/$(git rev-parse --abbrev-ref HEAD)}
    export GIT_COMMIT=${GIT_COMMIT:-$(git rev-parse HEAD)}
else
    export GIT_URL=https://github.com/${GITHUB_REPOSITORY}.git
    export GIT_BRANCH=origin/$(basename ${GITHUB_REF})
    export GIT_COMMIT=${GITHUB_SHA}
fi

BUILD_IMAGE_NAME="${BUILD_IMAGE_NAMES[$DRIVER_NAME-centos6-default]}"
echo $BUILD_IMAGE_NAME
docker pull "${BUILD_IMAGE_NAME}"
docker run \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e LOCAL_USER_ID=$(id -u $USER) \
        -e WHITESOURCE_API_KEY \
        -e GIT_URL \
        -e GIT_BRANCH \
        -e GIT_COMMIT \
        -e BUILD_TYPE \
        -e BUILD_CLEAN \
        -e AWS_ACCESS_KEY_ID \
        -e AWS_SECRET_ACCESS_KEY \
        -e GITHUB_ACTIONS \
        -e GITHUB_SHA \
        -e GITHUB_EVENT_NAME \
        -e GITHUB_REF \
        -w /mnt/host \
        "${BUILD_IMAGE_NAME}" \
        "/mnt/host/ci/build/build.sh"

