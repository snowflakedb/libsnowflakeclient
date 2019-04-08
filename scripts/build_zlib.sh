#!/bin/bash -e
#
# Build zlib
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Builds zlib"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SOURCE_DIR=$DIR/../deps/zlib-1.2.11
source $DIR/_init.sh $@

# init environment
#init_environment $DIR

# build
BUILD_DIR=../deps-build/linux/zlib
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
    if [ -e "Makefile" ] ; then
        make distclean clean > /dev/null || true
    fi
    ./configure ${zlib_config_opts[@]} > /dev/null || true
    make install > /dev/null || true
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi
