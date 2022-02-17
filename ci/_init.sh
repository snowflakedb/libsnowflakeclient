#!/bin/bash -e

sys=$(echo $(uname) | tr '[:upper:]' '[:lower:]')
export ARCH=$(uname -p)
if [[ "$sys" == "linux" ]] && [[ "$ARCH" != "x86_64" ]]; then
    export PLATFORM=$sys-$ARCH
    export IMAGE_OS=centos8-$ARCH
else
    export PLATFORM=$sys
    export IMAGE_OS=centos6-default
fi

export INTERNAL_REPO=nexus.int.snowflakecomputing.com:8086
if [[ -z "$GITHUB_ACTIONS" ]]; then
    # Use the internal Docker Registry
    export DOCKER_REGISTRY_NAME=$INTERNAL_REPO/docker
    export WORKSPACE=${WORKSPACE:-/tmp}
else
    # Use Docker Hub
    export DOCKER_REGISTRY_NAME=snowflakedb
    export WORKSPACE=$GITHUB_WORKSPACE
fi

export DRIVER_NAME=libsnowflakeclient

# Build images
BUILD_IMAGE_VERSION=1

# Test Images
TEST_IMAGE_VERSION=1

declare -A BUILD_IMAGE_NAMES=(
    [$DRIVER_NAME-$IMAGE_OS]=$DOCKER_REGISTRY_NAME/client-$DRIVER_NAME-$IMAGE_OS-build:$BUILD_IMAGE_VERSION
    [$DRIVER_NAME-$IMAGE_OS]=$DOCKER_REGISTRY_NAME/client-$DRIVER_NAME-$IMAGE_OS-build:$BUILD_IMAGE_VERSION
)
export BUILD_IMAGE_NAMES

declare -A TEST_IMAGE_NAMES=(
    [$DRIVER_NAME-$IMAGE_OS]=$DOCKER_REGISTRY_NAME/client-$DRIVER_NAME-$IMAGE_OS-test:$BUILD_IMAGE_VERSION
)

export TEST_IMAGE_NAMES
