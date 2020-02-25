#!/bin/bash -e
#
# Test libsnowflakeclient for Linux
#
# - TEST_IMAGE_NAME - the target Docker image key. It must be registered in _init.sh
#

set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/_init.sh

echo $GIT_COMMIT
echo $GIT_BRANCH
echo $libsnowflake_version

cd $WORKSPACE
aws s3 cp s3://sfc-jenkins/repository/libsnowflakeclient/linux/${GIT_BRANCH}/${GIT_COMMIT}/libsnowflakeclient_linux_Debug_$libsnowflake_version.tgz libsnowflakeclient_linux_Debug_$libsnowflake_version.tgz
tar zxvf libsnowflakeclient_linux_Debug_$libsnowflake_version.tgz

docker pull "${TEST_IMAGE_NAME}"

docker run \
        -t \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e LOCAL_USER_ID=$(id -u $USER) \
        -e AWS_ACCESS_KEY_ID \
        -e AWS_SECRET_ACCESS_KEY \
        "${TEST_IMAGE_NAME}" \
        "/mnt/host/scripts/run_tests.sh"

echo "PARAM: ${GIT_BRANCH , ${GIT_COMMIT} , ${libsnowflake_version}"
