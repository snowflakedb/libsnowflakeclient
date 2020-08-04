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
else
    echo "[ERROR] $PLATFORM is not supported"
fi
