#!/bin/bash -e
#
# Build ARROW library
#
set -o pipefail

#Change the version in the arrow.mk to build 
ARROW_VERSION=0.15.0

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [[ "$PLATFORM" == "linux" ]]; then
    cp $THIS_DIR/arrow.mk ../deps/
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
    download_from_sfc_jenkins arrow 0.15.0 Release
fi
