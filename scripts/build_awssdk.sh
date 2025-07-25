#!/bin/bash -e
#
# build aws-cpp-sdk
# GitHub repo: https://github.com/aws/aws-sdk-cpp.git
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build AWS SDK"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

AWS_SRC_VERSION=1.11.500
AWS_BUILD_VERSION=1
AWS_DIR=aws-sdk-cpp-$AWS_SRC_VERSION
AWS_VERSION=$AWS_SRC_VERSION.$AWS_BUILD_VERSION

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $AWS_VERSION && exit 0

OPENSSL_BUILD_DIR=$DEPENDNCY_DIR/openssl
LIBCURL_BUILD_DIR=$DEPENDNCY_DIR/curl
AWS_SOURCE_DIR=$DEPS_DIR/${AWS_DIR}
AWS_CMAKE_BUILD_DIR=$AWS_SOURCE_DIR/cmake-build-$target
AWS_BUILD_DIR=$DEPENDENCY_DIR/aws

rm -rf $AWS_SOURCE_DIR
git clone --single-branch --branch $AWS_SRC_VERSION --recursive https://github.com/aws/aws-sdk-cpp.git $AWS_SOURCE_DIR
pushd $AWS_SOURCE_DIR
  git apply ../../patches/aws-sdk-cpp-$AWS_SRC_VERSION.patch
popd

[[ -n "$DOWNLOAD_ONLY" ]] && exit 0

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
    "-DBUILD_ONLY=s3;sts"
    "-DCMAKE_INSTALL_PREFIX=$AWS_BUILD_DIR"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DCMAKE_PREFIX_PATH=\"$LIBCURL_BUILD_DIR/;$OPENSSL_BUILD_DIR/\""
    "-DENABLE_TESTING=OFF"
    "-DENABLE_CURL_LOGGING=OFF"
#disable CPU extentsions to fix build error on Linux
#CPU extentsions might not be always available on all customer environments
    "-DUSE_CPU_EXTENSIONS=OFF"
    "-DOPENSSL_ROOT_DIR=$DEPENDENCY_DIR/openssl"
    "-Dcrypto_INCLUDE_DIR=$DEPENDENCY_DIR/openssl/include"
    "-Dcrypto_LIBRARY=$DEPENDENCY_DIR/openssl/lib/libcrypto.a"
    "-DOPENSSL_USE_STATIC_LIBS=true"
    "-DCURL_INCLUDE_DIR=$DEPENDENCY_DIR/curl/include"
    "-DCURL_LIBRARY=$DEPENDENCY_DIR/curl/lib/libcurl.a"
    "-DZLIB_INCLUDE_DIR=$DEPENDENCY_DIR/zlib/include"
    "-DZLIB_LIBRARY=$DEPENDENCY_DIR/zlib/lib/libz.a"
)

ADDITIONAL_CXXFLAGS=
if [[ "$PLATFORM" == "linux" ]]; then
    if [[ "$GCCVERSION" > "9" ]]; then
        ADDITIONAL_CXXFLAGS="-Wno-error=deprecated-copy"
    fi
fi

# Check to see if we are doing a universal build or not.
# If we are not doing a universal build, pick an arch to
# build
if [[ "$PLATFORM" == "darwin" ]]; then
    if [[ "$ARCH" == "universal" ]]; then
        echo "[INFO] Building Universal Binary"
        aws_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64")
    elif [[ "$ARCH" == "x86" ]]; then
        echo "[INFO] Building x86 Binary"
        aws_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=i386")
    elif [[ "$ARCH" == "x64" ]]; then
        echo "[INFO] Building x64 Binary"
        aws_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64")
    else
        echo "[INFO] Building $ARCH Binary"
        aws_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=$ARCH")
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

# on arm64 linux, the aws lib might be installed to lib folder
if [[ "$PLATFORM" == "linux" ]] && [[ ! -d "$AWS_BUILD_DIR/lib64" ]]; then
    mv -f $AWS_BUILD_DIR/lib $AWS_BUILD_DIR/lib64
fi

echo === zip_file "aws" "$AWS_VERSION" "$target"
zip_file "aws" "$AWS_VERSION" "$target"
