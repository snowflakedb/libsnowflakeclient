#!/bin/bash -e
#
# build arrow on linux
# GitHub repo: https://github.com/apache/arrow.git
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build ARROW"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $ARROW_VERSION && exit 0

ARROW_SOURCE_DIR=$DEPS_DIR/arrow-$ARROW_SRC_VERSION
ARROW_BUILD_DIR=$DEPENDENCY_DIR/arrow
ARROW_DEPS_BUILD_DIR=$DEPENDENCY_DIR/arrow_deps
ARROW_CMAKE_BUILD_DIR=$ARROW_SOURCE_DIR/cpp/cmake-build

ARROW_CXXFLAGS="-O2 -fPIC -pthread"
arrow_configure_opts=()
if [[ "$target" != "Release" ]]; then
    arrow_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
    ARROW_CMAKE_BUILD_DIR=$ARROW_SOURCE_DIR/cpp/cmake-build-debug
    if [[ "$PLATFORM" == "darwin" ]]; then
        ARROW_CXXFLAGS="$ARROW_CXXFLAGS -Wno-error=unused-const-variable -Wno-error=unneeded-internal-declaration -Wno-error=deprecated-declarations"
    fi
else
    arrow_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
arrow_configure_opts+=(
    "-DARROW_USE_STATIC_CRT=ON"
    "-DCMAKE_C_COMPILER=$CC"
    "-DCMAKE_CXX_COMPILER=$CXX"
    "-DCMAKE_INSTALL_PREFIX=$ARROW_BUILD_DIR"
    "-DARROW_BOOST_USE_SHARED=OFF"
    "-DARROW_BUILD_SHARED=OFF"
    "-DARROW_BUILD_STATIC=ON"
    "-DBOOST_SOURCE=SYSTEM"
    "-DARROW_WITH_BROTLI=OFF"
    "-DARROW_WITH_LZ4=OFF"
    "-DARROW_WITH_SNAPPY=OFF"
    "-DARROW_WITH_ZLIB=OFF"
    "-DARROW_JSON=OFF"
    "-DARROW_DATASET=OFF"
    "-DARROW_BUILD_UTILITIES=OFF"
    "-DARROW_COMPUTE=OFF"
    "-DARROW_FILESYSTEM=OFF"
    "-DARROW_USE_GLOG=OFF"
    "-DARROW_HDFS=OFF"
    "-DARROW_SIMD_LEVEL=NONE"
    "-DARROW_WITH_BACKTRACE=OFF"
    "-DARROW_JEMALLOC_USE_SHARED=OFF"
    "-DARROW_BUILD_TESTS=OFF"
    "-DBoost_INCLUDE_DIR=$DEPENDENCY_DIR/boost/include"
    "-DBOOST_SYSTEM_LIBRARY=$DEPENDENCY_DIR/boost/lib/libboost_system.a"
    "-DBOOST_FILESYSTEM_LIBRARY=$DEPENDENCY_DIR/boost/lib/libboost_filesystem.a"
    "-DBOOST_REGEX_LIBRARY=$DEPENDENCY_DIR/boost/lib/libboost_regex.a"
)

rm -rf $ARROW_BUILD_DIR
rm -rf $ARROW_DEPS_BUILD_DIR
rm -rf $ARROW_CMAKE_BUILD_DIR
mkdir $ARROW_BUILD_DIR
mkdir $ARROW_DEPS_BUILD_DIR
mkdir $ARROW_DEPS_BUILD_DIR/lib
mkdir $ARROW_CMAKE_BUILD_DIR

cd $ARROW_CMAKE_BUILD_DIR
# If we are not doing a universal build, build with 64-bit
if [[ "$PLATFORM" == "darwin" ]] && [[ "$ARCH" == "universal" ]]; then
    arrow_configure_opts+=(
        "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
    )
    export CXXFLAGS="-arch arm64 -arch x86_64"
    export CFLAGS="-arch arm64 -arch x86_64"
fi
$CMAKE -E env $CMAKE ${arrow_configure_opts[@]} -DARROW_CXXFLAGS="$ARROW_CXXFLAGS" ../

make
make install

#make install does not do much here
cp -f $ARROW_CMAKE_BUILD_DIR/jemalloc_ep-prefix/src/jemalloc_ep/lib/*.a $ARROW_DEPS_BUILD_DIR/lib/
# on arm64 linux, the arrow lib might be installed to lib folder
if [[ "$PLATFORM" == "linux" && ! -d "$ARROW_BUILD_DIR/lib64" ]]; then
    mv -f $ARROW_BUILD_DIR/lib $ARROW_BUILD_DIR/lib64
fi

cd $DIR

echo === zip_files "arrow" "$ARROW_VERSION" "$target" "arrow arrow_deps boost"
zip_files "arrow" "$ARROW_VERSION" "$target" "arrow arrow_deps boost"

if [[ -n "$GITHUB_ACTIONS" ]]; then
    rm -rf $ARROW_SOURCE_DIR
fi
