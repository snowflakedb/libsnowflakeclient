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

OOB_SRC_VERSION=1.0.4
OOB_BUILD_VERSION=11
OOB_VERSION=$OOB_SRC_VERSION.$OOB_BUILD_VERSION
UUID_VERSION=2.39.0

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $OOB_VERSION && exit 0

export OOB_SOURCE_DIR=$DEPS_DIR/oob-$OOB_SRC_VERSION/

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
export CURL_DIR=curl
export UUID_DIR=util-linux-$UUID_VERSION
if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    export AR=ar
    export AROPTIONS=rcs
    export CFLAGS="-D_LARGEFILE64_SOURCE"
    make distclean clean > /dev/null || true
    make LIB=libtelemetry.a
elif [[ "$PLATFORM" == "darwin" ]]; then
    export CC=clang
    export AR=libtool
    export AROPTIONS="-static -o"
    make distclean > /dev/null || true
    if [[ "$ARCH" == "x86" || "$ARCH" == "x64" || "$ARCH" == "universal" ]]; then
        # for intel always universal
        export CFLAGS="-mmacosx-version-min=10.14 -arch x86_64 -Xarch_x86_64"
        make LIB=libtelemetryx64.a
        make clean > /dev/null || true
        export CFLAGS="-mmacosx-version-min=10.14 -arch arm64 -Xarch_arm64"
        make LIB=libtelemetryarm64.a
        lipo -create $OOB_SOURCE_DIR/libtelemetryx64.a $OOB_SOURCE_DIR/libtelemetryarm64.a -output $OOB_SOURCE_DIR/libtelemetry.a
    else
        export CFLAGS="-mmacosx-version-min=10.14 -arch $ARCH  -Xarch_$ARCH"
        make LIB=libtelemetry.a
    fi
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi

cp $OOB_SOURCE_DIR/*.h $OOB_BUILD_DIR/include
cp $OOB_SOURCE_DIR/libtelemetry.a $OOB_BUILD_DIR/lib

echo === zip_file "oob" "$OOB_VERSION" "$target"
zip_file "oob" "$OOB_VERSION" "$target"
