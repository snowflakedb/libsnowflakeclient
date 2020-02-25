#!/bin/bash -e
#
# Build zlib
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Builds zlib"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v: return version"
    exit 2
}
set -o pipefail

ZLIB_VERSION=1.2.11

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SOURCE_DIR=$DIR/../deps/zlib-${ZLIB_VERSION}
source $DIR/_init.sh $@

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

cd $SOURCE_DIR

if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    export CC=gcc52
    make -f Makefile.in distclean > /dev/null || true
    ./configure ${zlib_config_opts[@]} > /dev/null || true
    make install > /dev/null || true

elif [[ "$PLATFORM" == "darwin" ]]; then
   echo "Now building for x86_64"
   export CFLAGS="-fPIC -arch x86_64 -mmacosx-version-min=10.12"
   BUILD_DIR_64=$BUILD_DIR/zlib_64
   make -f Makefile.in distclean > /dev/null || true
   ./configure -s --static --prefix=$BUILD_DIR_64 
   make install

   echo "Now building for i386"
   make -f Makefile.in distclean > /dev/null || true
   cd $SOURCE_DIR
   CFLAGS=""
   LDFLAGS=""
   CXXFLAGS=""
   export CFLAGS="-fPIC -arch i386 -mmacosx-version-min=10.12" 
   BUILD_DIR_32=$BUILD_DIR/zlib_32
  ./configure -s --static --prefix=$BUILD_DIR_32  || exit 1
   make install

   mkdir -p $BUILD_DIR/lib
   mkdir -p $BUILD_DIR/include
   cp -fr $BUILD_DIR_32/include/* $BUILD_DIR/include
   lipo -create $BUILD_DIR_64/lib/libz.a $BUILD_DIR_32/lib/libz.a -output $BUILD_DIR/lib/libz.a 
   rm -rf $BUILD_DIR_64 $BUILD_DIR_32
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi
