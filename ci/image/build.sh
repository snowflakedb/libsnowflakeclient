#!/bin/bash -ex
#
# Build Docker images
#
set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/../_init.sh

# Sometime we want to specify names instead
if [[ $# -ge 1 ]]; then
   for (( i=1; i<="$#"; i++ )) do
      name="${!i}"
      docker build \
        --file $THIS_DIR/Dockerfile.$name-build \
        --label snowflake \
        --label $DRIVER_NAME \
        --tag ${BUILD_IMAGE_NAMES[$name]} .
   done
   exit 
fi

 
for name in "${!BUILD_IMAGE_NAMES[@]}"; do
    echo $name
    docker build \
        --file $THIS_DIR/Dockerfile.$name-build \
        --label snowflake \
        --label $DRIVER_NAME \
        --tag ${BUILD_IMAGE_NAMES[$name]} .
done

for name in "${!TEST_IMAGE_NAMES[@]}"; do
    docker build \
        --file $THIS_DIR/Dockerfile.$name-test \
        --label snowflake \
        --label $DRIVER_NAME \
        --tag ${TEST_IMAGE_NAMES[$name]} .
done
