#!/bin/bash -e

export PLATFORM=$(echo $(uname) | tr '[:upper:]' '[:lower:]')
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
BUILD_IMAGE_VERSION_X64=2
BUILD_IMAGE_VERSION_AARCH64=3

# Test Images
TEST_IMAGE_VERSION_X64=1
TEST_IMAGE_VERSION_AARCH64=3

PLATFORM_ARCH=$(uname -p)
if [[ -z "$TARGET_PLATFORM" ]]; then
  if [[ "$PLATFORM_ARCH" == "aarch64" ]]; then
    export TARGET_PLATFORM=arm64
  else
    export TARGET_PLATFORM=amd64
  fi
fi
if [[ "$TARGET_PLATFORM" == "arm64" ]]; then
  export DOCKER_MARK="ubuntu20-aarch64"
  declare -A BUILD_IMAGE_NAMES=(
    [$DRIVER_NAME-$DOCKER_MARK]=$DOCKER_REGISTRY_NAME/client-$DRIVER_NAME-ubuntu20-aarch64:$BUILD_IMAGE_VERSION_AARCH64
  )

  declare -A TEST_IMAGE_NAMES=(
    [$DRIVER_NAME-$DOCKER_MARK]=$DOCKER_REGISTRY_NAME/client-$DRIVER_NAME-ubuntu20-aarch64:$TEST_IMAGE_VERSION_AARCH64
  )
else
  export DOCKER_MARK="centos7-default"
  declare -A BUILD_IMAGE_NAMES=(
    [$DRIVER_NAME-$DOCKER_MARK]=$DOCKER_REGISTRY_NAME/client-$DRIVER_NAME-centos7-default-build:$BUILD_IMAGE_VERSION_X64
  )

  declare -A TEST_IMAGE_NAMES=(
    [$DRIVER_NAME-$DOCKER_MARK]=$DOCKER_REGISTRY_NAME/client-$DRIVER_NAME-centos7-default-test:$TEST_IMAGE_VERSION_X64
  )
fi

export BUILD_IMAGE_NAMES

export TEST_IMAGE_NAMES
