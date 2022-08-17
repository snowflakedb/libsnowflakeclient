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

ARROW_VERSION=0.17.1
#The full version number for dependency packaging/uploading/downloading
ARROW_DEP_VERSION=${ARROW_VERSION}.1

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd)"
DEPS_BUILD_DIR=$DIR/../deps-build
source $DIR/_init.sh $@

[[ -n "$GET_VERSION" ]] && echo $ARROW_DEP_VERSION && exit 0

if [[ -n "$ARROW_FROM_SOURCE" ]]; then
    $DIR/build_boost_source.sh -t $target
    $DIR/build_arrow_source.sh -t $target
    exit 0
fi

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

cd $DIR
echo === zip_files "arrow" "$ARROW_DEP_VERSION" "$target" "arrow arrow_deps boost"
zip_files "arrow" "$ARROW_DEP_VERSION" "$target" "arrow arrow_deps boost"
