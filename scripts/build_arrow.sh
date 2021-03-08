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
ARROW_VERSION=0.17.0

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $ARROW_VERSION && exit 0

ARROW_SOURCE_DIR=$DEPS_DIR/arrow
ARROW_BUILD_DIR=$DEPENDENCY_DIR/arrow
ARROW_CMAKE_BUILD_DIR=$ARROW_SOURCE_DIR/cmake-build

GIT_REPO="https://github.com/apache/arrow.git"
CLONE_CMD="git clone -b master $GIT_REPO $ARROW_SOURCE_DIR"

if [ ! -d $ARROW_SOURCE_DIR ]; then
  n=0
  # retry 5 times on cloning
  until [ $n -ge 5 ]
  do
    if $CLONE_CMD ; then
      break
    fi
    n=$[$n+1]
  done

  if [ ! -d $ARROW_SOURCE_DIR ]; then
    echo "[Error] failed to clone repo from $GIT_REPO"
    exit 1
  fi

  cd $ARROW_SOURCE_DIR
  git checkout tags/apache-arrow-$ARROW_VERSION -b apache-arrow-$ARROW_VERSION || true
else
  cd $ARROW_SOURCE_DIR
  git fetch || true
  git checkout tags/apache-arrow-$ARROW_VERSION -b apache-arrow-$ARROW_VERSION || true
fi

if [[ "$PLATFORM" == "linux" ]]; then
    cp $DIR/arrow.mk $DIR/../deps/
    cd $DIR/../deps/
    BUILD_TYPE=$target CC=gcc52 CXX=g++52 make -f arrow.mk
else
    echo "[ERROR] $PLATFORM is not supported"
fi
