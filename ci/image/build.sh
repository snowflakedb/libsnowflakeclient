#!/bin/bash -ex
#
# Build Docker images
#
set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/../_init.sh

for name in "${!BUILD_IMAGE_NAMES[@]}"; do
    echo $name
    docker buildx build --platform linux/$TARGET_PLATFORM \
        --file $THIS_DIR/Dockerfile.$name-build \
        --label snowflake \
        --label $DRIVER_NAME \
        --tag ${BUILD_IMAGE_NAMES[$name]} .
done

for name in "${!TEST_IMAGE_NAMES[@]}"; do
    docker buildx build --platform linux/$TARGET_PLATFORM \
        --file $THIS_DIR/Dockerfile.$name-test \
        --label snowflake \
        --label $DRIVER_NAME \
        --tag ${TEST_IMAGE_NAMES[$name]} .
done
