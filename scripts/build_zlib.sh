#!/bin/bash -e
#
# Build zlib
# GitHub repo: https://github.com/madler/zlib.git
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Builds zlib"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

ZLIB_SRC_VERSION=1.3.1
ZLIB_BUILD_VERSION=7
ZLIB_VERSION=$ZLIB_SRC_VERSION.$ZLIB_BUILD_VERSION

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $ZLIB_VERSION && exit 0

# build
BUILD_DIR=$DEPENDENCY_DIR/zlib
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

zlib_config_opts=()
zlib_config_opts+=(
    "--static"
    "--prefix=$BUILD_DIR"
)

SOURCE_DIR=$DIR/../deps/zlib-${ZLIB_SRC_VERSION}
cd $SOURCE_DIR

if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    export CFLAGS="-fPIC -m32"
    make -f Makefile.in distclean > /dev/null || true
    ./configure ${zlib_config_opts[@]} > /dev/null || true
    make install > /dev/null || true

elif [[ "$PLATFORM" == "darwin" ]]; then
   if [[ "$ARCH" == "x86" || "$ARCH" == "x64" || "$ARCH" == "universal" ]]; then
       # for intel always universal
       echo "Now building for x86_64"
       export CFLAGS="-fPIC -arch x86_64 -mmacosx-version-min=${MACOSX_VERSION_MIN}"
       BUILD_DIR_X64=$BUILD_DIR/zlib_x64
       make -f Makefile.in distclean > /dev/null || true
       ./configure -s --static --prefix=$BUILD_DIR_X64
       make install

       echo "Now building for arm64"
       make -f Makefile.in distclean > /dev/null || true
       cd $SOURCE_DIR
       CFLAGS=""
       LDFLAGS=""
       CXXFLAGS=""
       export CFLAGS="-fPIC -arch arm64 -mmacosx-version-min=${MACOSX_VERSION_MIN}"
       BUILD_DIR_ARM64=$BUILD_DIR/zlib_arm64
      ./configure -s --static --prefix=$BUILD_DIR_ARM64  || exit 1
       make install

       mkdir -p $BUILD_DIR/{lib,include}
       cp -fr $BUILD_DIR_X64/include/* $BUILD_DIR/include
       lipo -create $BUILD_DIR_X64/lib/libz.a $BUILD_DIR_ARM64/lib/libz.a -output $BUILD_DIR/lib/libz.a
       rm -rf $BUILD_DIR_X64 $BUILD_DIR_ARM64
    else
       echo "Now building for $ARCH"
       export CFLAGS="-fPIC -arch $ARCH -mmacosx-version-min=${MACOSX_VERSION_MIN}"
       make -f Makefile.in distclean > /dev/null || true
       ./configure -s --static --prefix=$BUILD_DIR
       make install
    fi
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi
echo === zip_file "zlib" "$ZLIB_VERSION" "$target"
zip_file "zlib" "$ZLIB_VERSION" "$target"
