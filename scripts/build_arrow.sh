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

# Change the version in arrow.mk to build
ARROW_VERSION=0.17.1

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd)"
DEPS_BUILD_DIR=$DIR/../deps-build
source $DIR/_init.sh $@

[[ -n "$GET_VERSION" ]] && echo $ARROW_VERSION && exit 0

cd $DIR/../deps-build
if [ -d "arrow" ]; then rm -rf arrow; fi
if [ -d "arrow_deps" ]; then rm -rf arrow_deps; fi
if [ -d "boost" ]; then rm -rf boost; fi
tar xzf arrow_${PLATFORM}_${target}-${ARROW_VERSION}.tar.gz
if [ -d "$DEPENDENCY_DIR/arrow" ]; then rm -rf $DEPENDENCY_DIR/arrow; fi
if [ -d "$DEPENDENCY_DIR/arrow_deps" ]; then rm -rf $DEPENDENCY_DIR/arrow_deps; fi
if [ -d "$DEPENDENCY_DIR/boost" ]; then rm -rf $DEPENDENCY_DIR/boost; fi
if [ -d "arrow" ]; then mv arrow $DEPENDENCY_DIR; fi
if [ -d "arrow_deps" ]; then mv arrow_deps $DEPENDENCY_DIR; fi
if [ -d "boost" ]; then mv boost $DEPENDENCY_DIR; fi

