#!/bin/bash -e
#
# build aws-cpp-sdk
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build AWS SDK"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

AWS_VERSION=1.3.50

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $AWS_VERSION && exit 0

OPENSSL_BUILD_DIR=$DEPENDNCY_DIR/openssl
LIBCURL_BUILD_DIR=$DEPENDNCY_DIR/curl
AWS_SOURCE_DIR=$DEPS_DIR/aws-sdk-cpp-${AWS_VERSION}
AWS_CMAKE_BUILD_DIR=$AWS_SOURCE_DIR/cmake-build-$target
AWS_BUILD_DIR=$DEPENDENCY_DIR/aws

aws_configure_opts=()
if [[ "$target" != "Release" ]]; then
    aws_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
else
    aws_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
aws_configure_opts+=(
    "-DCMAKE_C_COMPILER=$GCC"
    "-DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF"
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

ADDITIONAL_CXXFLAGS=
# Check to see if we are doing a universal build or not.
# If we are not doing a universal build, pick an arch to
# build
if [[ "$PLATFORM" == "darwin" ]]; then
    if [[ "$ARCH" == "universal" ]]; then
        echo "[INFO] Building Universal Binary"
        aws_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;i386")
    elif [[ "$ARCH" == "x86" ]]; then
        echo "[INFO] Building x86 Binary"
        aws_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=i386")
    else
        echo "[INFO] Building x64 Binary"
        aws_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64")
    fi
    ADDITIONAL_CXXFLAGS="-mmacosx-version-min=${MACOSX_VERSION_MIN}"
fi

rm -rf $AWS_BUILD_DIR
rm -rf $AWS_CMAKE_BUILD_DIR
mkdir $AWS_BUILD_DIR
mkdir $AWS_CMAKE_BUILD_DIR

# Keep GIT_DIR for the issue https://github.com/aws/aws-sdk-cpp/issues/383
export GIT_DIR=/tmp

cd $AWS_CMAKE_BUILD_DIR
$CMAKE -E env CXXFLAGS=$ADDITIONAL_CXXFLAGS $CMAKE ${aws_configure_opts[@]} ../

unset GIT_DIR

make
make install

echo === zip_file "aws" "$AWS_VERSION" "$target"
zip_file "aws" "$AWS_VERSION" "$target"
