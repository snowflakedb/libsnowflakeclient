#!/bin/bash -e
INTERNAL_REPO=nexus.int.snowflakecomputing.com:8086
INTERNAL_CLIENT_REPO=$INTERNAL_REPO/docker/client
BUILD_IMAGE_VERSION=1
BUILD_IMAGE_NAME=$INTERNAL_CLIENT_REPO/libsnowflakeclient-linux-centos6-build:$BUILD_IMAGE_VERSION
TEST_IMAGE_VERSION=1
TEST_IMAGE_NAME=$INTERNAL_CLIENT_REPO/libsnowflakeclient-linux-centos6-build:$BUILD_IMAGE_VERSION

NEXUS_USER=${USERNAME:-jenkins}
if [[ -z "$NEXUS_PASSWORD" ]]; then
    echo "[ERROR] Set NEXUS_PASSWORD to your LDAP password to access the internal repository!"
    exit 1
fi
if ! docker login --username "$NEXUS_USER" --password "$NEXUS_PASSWORD" $INTERNAL_REPO; then
    echo "[ERROR] Failed to connect to the nexus server. Verify the environment variable NEXUS_PASSWORD is set correctly for NEXUS_USER: $NEXUS_USER"
    exit 1
fi
if [[ -z "$AWS_ACCESS_KEY_ID" ]]; then
    echo "[ERROR] Set AWS_ACCESS_KEY_ID to your LDAP password to access the internal repository!"
    exit 1
fi
if [[ -z "$AWS_SECRET_ACCESS_KEY" ]]; then
    echo "[ERROR] Set NEXUS_PASSWORD to your LDAP password to access the internal repository!"
    exit 1
fi
