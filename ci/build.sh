#!/bin/bash -e
#
# Build libsnowflakeclient
#
set -o pipefail
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $THIS_DIR/_init.sh

export GIT_COMMIT=`git rev-parse HEAD`
libsnowflake_ver='0.4.5'


docker pull "${BUILD_IMAGE_NAME}"

docker run \
        -t \
        -v $(cd $THIS_DIR/.. && pwd):/mnt/host \
        -v $WORKSPACE:/mnt/workspace \
        -e LOCAL_USER_ID=$(id -u $USER) \
        -e AWS_ACCESS_KEY_ID \
        -e AWS_SECRET_ACCESS_KEY \
        "${BUILD_IMAGE_NAME}" \
        "/mnt/host/scripts/build_libsnowflakeclient.sh"

cd $WORKSPACE
tar zcvf libsnowflakeclient_linux_Debug_$libsnowflake_ver.tgz cmake-build
tar zcvf libsnowflakeclient_linux_Release_$libsnowflake_ver.tgz -C $WORKSPACE/include $WORKSPACE/cmake-build 
echo $GIT_COMMIT > latest_commit

aws s3 cp $WORKSPACE/libsnowflakeclient_linux_Debug_$libsnowflake_ver.tgz s3://sfc-jenkins/repository/libsnowflakeclient/linux/${GIT_BRANCH}/${GIT_COMMIT}/libsnowflakeclient_linux_Debug_$libsnowflake_ver.tgz
aws s3 cp $WORKSPACE/libsnowflakeclient_linux_Release_$libsnowflake_ver.tgz s3://sfc-jenkins/repository/libsnowflakeclient/linux/${GIT_BRANCH}/${GIT_COMMIT}/libsnowflakeclient_linux_Release_$libsnowflake_ver.tgz
aws s3 cp $WORKSPACE/latest_commit s3://sfc-jenkins/repository/libsnowflakeclient/linux/${GIT_BRANCH}/latest_commit

echo "PARAM: ${GIT_BRANCH} , ${GIT_COMMIT} "

cat <<PARAMS > $WORKSPACE/build_properties.txt
GIT_BRANCH=$GIT_BRANCH
GIT_COMMIT=$GIT_COMMIT
libsnowflake_version=$libsnowflake_ver
PARAMS