

#!/bin/bash -e
#
# Build libuuid
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Builds libuuid"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}

set -o pipefail

if [[ "$PLATFORM" == "linux" ]]; then
    export GXX=g++52
    export CXX=g++52
    export CC=gcc52
    export GCC=gcc52
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SOURCE_DIR=$DIR/../deps/
source $DIR/_init.sh $@

# init environment
#init_environment $DIR

# build
UTIL_LINUX_TAR_GZ=$DEPS_DIR/util-linux.tar.gz
SOURCE_DIR=$DEPS_DIR/util-linux

tar -xzf $UTIL_LINUX_TAR_GZ -C $DEPS_DIR

if [ ! -d "$SOURCE_DIR" ] ; then
    echo "Could not extract $UTIL_LINUX_TAR_GZ" 
    exit 1
fi

cd $SOURCE_DIR

BUILD_DIR=$DEPS_DIR/../deps-build/$PLATFORM/uuid
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

if [[ "$PLATFORM" == "linux" ]]; then
    echo "Generating UUID build system"
    ./autogen.sh || true
#    Make sure the compiled binary is position independent as ODBC is a shared library
    export CFLAGS="-fPIC"
    export AL_OPTS="-I/usr/share/aclocal"
    ./configure --disable-all-programs --enable-libuuid --prefix=$DEPENDENCY_DIR/uuid  || true
    echo "Compiling UUID source"
    make install  || true

elif [[ "$PLATFORM" == "darwin" ]]; then

   export AL_OPTS="-I/usr/local/Cellar/automake/1.16.1_1/bin "
   BUILD_DIR_64=$BUILD_DIR/uuid_64
   make distclean > /dev/null 2>&1
   if [ -e "Makefile" ] ; then 
       make distclean > /dev/null || true
   fi
   echo "Generating UUID build system for 64 bit"
   export CFLAGS="-fPIC -arch x86_64 -Xarch_x86_64 -DSIZEOF_LONG_INT=8 -mmacosx-version-min=10.12"
   ./autogen.sh || true
   ./configure --disable-all-programs --enable-libuuid --prefix=$BUILD_DIR_64  || true
   echo "Compiling UUID source"
   make install  || true


   make distclean > /dev/null 2>&1
   if [ -e "Makefile" ] ; then 
       make distclean > /dev/null || true
   fi
   echo "Generating UUID build system"
   export CFLAGS="-fPIC -arch i386 -Xarch_i386 -DSIZEOF_LONG_INT=4 -mmacosx-version-min=10.12" 
   ./autogen.sh || true
   ./configure --disable-all-programs --enable-libuuid --prefix=$BUILD_DIR  || true
   echo "Compiling UUID source for 32 bit"
   make install  || true
   mv $BUILD_DIR/lib/libuuid.a $BUILD_DIR/lib/libuuid_32.a

   lipo -create $BUILD_DIR_64/lib/libuuid.a $BUILD_DIR/lib/libuuid_32.a -output $BUILD_DIR/lib/libuuid.a 
   rm -rf $BUILD_DIR_64 
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi

