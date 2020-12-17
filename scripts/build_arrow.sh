#!/bin/bash -e
#
# Build ARROW library
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build ARROW library"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

#Change the version in the arrow.mk to build 
ARROW_VERSION=0.15.0

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $ARROW_VERSION && exit 0

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [[ "$PLATFORM" == "linux" ]]; then
    mkdir ../deps
    cp $THIS_DIR/arrow.mk ../deps/arrow.mk
    cd ../deps/
    BUILD_TYPE=$BUILD_TYPE CC=gcc52 CXX=g++52 make -f arrow.mk
elif [[ "$PLATFORM" == "darwin" ]]; then
    echo "[INFO] For ${PLATFORM}, use pre-built binaries for Arrow"
    local zip_file_name=$(get_zip_file_name arrow $ARROW_VERSION $target)
    download_from_sfc_dev1_data arrow $ARROW_VERSION $target
    mv $THIS_DIR/../artifacts/$zip_file_name $THIS_DIR/../deps-build/$PLATFORM/$target
    tar xzf $THIS_DIR/../deps-build/$PLATFORM/$target/$zip_file_name
    rm $THIS_DIR/../deps-build/$PLATFORM/$target
else
    echo "[ERROR] $PLATFORM is not supported"
fi
