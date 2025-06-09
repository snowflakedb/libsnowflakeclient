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
    # remove unnecssary files to save disk space on github
    # base on the workaround provided here:
    # https://github.com/actions/runner-images/issues/2840#issuecomment-790492173
    sudo rm -rf /usr/share/dotnet
    sudo rm -rf /opt/ghc
    sudo rm -rf "/usr/local/share/boost"
fi

BUILD_IMAGE_NAME="${BUILD_IMAGE_NAMES[$DRIVER_NAME-$DOCKER_MARK]}"
echo $BUILD_IMAGE_NAME
docker pull "${BUILD_IMAGE_NAME}"
docker run \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e LOCAL_USER_ID=$(id -u $USER) \
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
        -e CLIENT_CODE_COVERAGE \
        -e USE_EXTRA_WARNINGS \
        -e LINK_TYPE \
        -e ENABLE_MOCK_OBJECTS \
        -w /mnt/host \
        "${BUILD_IMAGE_NAME}" \
        "/mnt/host/ci/build/build.sh"

#remove image to save disk space on github
if [[ -n "$GITHUB_ACTIONS" ]]; then
    docker rm -vf $(docker ps -aq --filter ancestor=${BUILD_IMAGE_NAME})
    if [[ $CLIENT_CODE_COVERAGE -ne 1 ]] && [[ "$BUILD_TYPE" != "Debug" ]]; then
        docker rmi -f "${BUILD_IMAGE_NAME}"
    fi
fi
