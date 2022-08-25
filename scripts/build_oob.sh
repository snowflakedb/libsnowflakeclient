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
OOB_BUILD_VERSION=16
OOB_VERSION=$OOB_SRC_VERSION.$OOB_BUILD_VERSION

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $OOB_VERSION && exit 0

export OOB_SOURCE_DIR=$DEPS_DIR/oob-$OOB_SRC_VERSION/

# staging cJSON for curl
LIBCURL_SOURCE_DIR=$DEPS_DIR/curl/
CURL_SRC_VERSION_GIT=${CURL_VERSION//./_}
rm -rf $LIBCURL_SOURCE_DIR
git clone --single-branch --branch curl-$CURL_SRC_VERSION_GIT --recursive https://github.com/curl/curl.git $LIBCURL_SOURCE_DIR
CJSON_SOURCE_DIR=$DEPS_DIR/cJSON-${CJSON_VERSION}
CJSON_PATCH=$DEPS_DIR/../patches/curl-cJSON-${CJSON_VERSION}.patch
rm -rf $CJSON_SOURCE_DIR
git clone https://github.com/DaveGamble/cJSON.git $CJSON_SOURCE_DIR
pushd $CJSON_SOURCE_DIR
  git checkout tags/v${CJSON_VERSION} -b ${CJSON_VERSION}
  git apply $CJSON_PATCH
  cp ./cJSON.c $LIBCURL_SOURCE_DIR/lib/vtls/sf_cJSON.c
  cp ./cJSON.h $LIBCURL_SOURCE_DIR/lib/vtls/sf_cJSON.h
popd

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
export UUID_DIR=uuid
if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    export CFLAGS="-m32"
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
    export AR=
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi

cp $OOB_SOURCE_DIR/*.h $OOB_BUILD_DIR/include
cp $OOB_SOURCE_DIR/libtelemetry.a $OOB_BUILD_DIR/lib

echo === zip_file "oob" "$OOB_VERSION" "$target"
zip_file "oob" "$OOB_VERSION" "$target"
