#!/bin/bash -e
#
# build aws-cpp-sdk
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build AWS SDK"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh
AWS_SOURCE_DIR=$DEPS_DIR/aws-sdk-cpp-1.3.50/
AWS_CMAKE_BUILD_DIR=$AWS_SOURCE_DIR/cmake-build
AWS_BUILD_DIR=$DEPENDENCY_DIR/aws

aws_configure_opts=()
if [[ "$target" != "Release" ]]; then
    aws_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
else
    aws_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
aws_configure_opts+=(
    "-DCMAKE_C_COMPILER=$GCC"
    "-DCMAKE_CXX_COMPILER=$GXX"
    "-DBUILD_ONLY=s3"
    "-DCMAKE_INSTALL_PREFIX=$AWS_BUILD_DIR"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DCMAKE_PREFIX_PATH=\"$LIBCURL_BUILD_DIR/;$OPENSSL_BUILD_DIR/\""
    "-DENABLE_TESTING=OFF"
    "-DOPENSSL_ROOT_DIR=$DEPENDENCY_DIR/openssl"
    "-DOPENSSL_USE_STATIC_LIBS=true"
    "-DCURL_INCLUDE_DIR=$DEPENDENCY_DIR/curl/include"
    "-DCURL_LIBRARY=$DEPENDENCY_DIR/curl/lib"
)

if [[ "$PLATFORM" == "darwin" ]]; then
    aws_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;i386")
fi

rm -rf $AWS_BUILD_DIR
rm -rf $AWS_CMAKE_BUILD_DIR
mkdir $AWS_BUILD_DIR
mkdir $AWS_CMAKE_BUILD_DIR

export GIT_DIR=/tmp

cd $AWS_CMAKE_BUILD_DIR
$CMAKE ${aws_configure_opts[@]} ../
make
make install
