#!/bin/bash -e
#
# Build oob
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Builds oob"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

OOB_VERSION=1.0.4

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $OOB_VERSION && exit 0

[[ ! -e "$DEPS_DIR/util-linux" ]] && tar xzf $DEPS_DIR/util-linux.tar.gz -C $DEPS_DIR
export OOB_SOURCE_DIR=$DEPS_DIR/oob-$OOB_VERSION/

# build
OOB_BUILD_DIR=$DEPENDENCY_DIR/oob
rm -rf $OOB_BUILD_DIR
mkdir -p $OOB_BUILD_DIR/{lib,include}

oob_config_opts=()
oob_config_opts+=(
    "--static"
    "--prefix=$OOB_BUILD_DIR"
)
cd $OOB_SOURCE_DIR
export CURL_DIR=curl-7.78.0
if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    if [[ -z "$XP_BUILD" ]] ; then
      export CC=gcc52
    else
      export CC=gcc82
    fi
    export AR=ar
    export AROPTIONS=rcs
    make distclean clean > /dev/null || true
    make LIB=libtelemetry.a
elif [[ "$PLATFORM" == "darwin" ]]; then
    export CC=clang
    export AR=libtool
    export AROPTIONS="-static -o"
    make distclean > /dev/null || true
    export CFLAGS="-mmacosx-version-min=10.12 -arch i386 -Xarch_i386 -DSIZEOF_LONG_INT=4 -Xarch_i386 -DHAVE_LONG_LONG" 
    make LIB=libtelemetry32.a 
    make clean > /dev/null || true
    export CFLAGS="-mmacosx-version-min=10.12 -arch x86_64 -Xarch_x86_64 -DSIZEOF_LONG_INT=8"
    make LIB=libtelemetry64.a 
    lipo -create $OOB_SOURCE_DIR/libtelemetry64.a $OOB_SOURCE_DIR/libtelemetry32.a -output $OOB_SOURCE_DIR/libtelemetry.a 
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi

cp $OOB_SOURCE_DIR/*.h $OOB_BUILD_DIR/include
cp $OOB_SOURCE_DIR/libtelemetry.a $OOB_BUILD_DIR/lib

echo === zip_file "oob" "$OOB_VERSION" "$target"
zip_file "oob" "$OOB_VERSION" "$target"
