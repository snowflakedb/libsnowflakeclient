#!/bin/bash -e
#
# build arrow on linux
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build ARROW"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

ARROW_VERSION=0.17.1

#If its not for XP use gcc52
if [[ -z "$XP_BUILD" ]] ; then
  export CC="/usr/lib64/ccache/gcc52"
  export CXX="/usr/lib64/ccache/g++52"
else
  export CC="gcc82"
  export CXX="g++82"
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $AZURE_VERSION && exit 0

ARROW_SOURCE_DIR=$DEPS_DIR/arrow
ARROW_BUILD_DIR=$DEPENDENCY_DIR/arrow
ARROW_DEPS_BUILD_DIR=$DEPENDENCY_DIR/arrow_deps
ARROW_CMAKE_BUILD_DIR=$ARROW_SOURCE_DIR/cpp/cmake-build

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
  git checkout tags/apache-arrow-$ARROW_VERSION -b v$ARROW_VERSION || true
else
  cd $ARROW_SOURCE_DIR
  git fetch || true
  git checkout tags/apache-arrow-$ARROW_VERSION -b v$ARROW_VERSION || true
fi


arrow_configure_opts=()
if [[ "$target" != "Release" ]]; then
    arrow_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
    ARROW_CMAKE_BUILD_DIR=$ARROW_SOURCE_DIR/cpp/cmake-build-debug
else
    arrow_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
arrow_configure_opts+=(
    "-DARROW_USE_STATIC_CRT=ON"
    "-DCMAKE_C_COMPILER=$CC"
    "-DCMAKE_CXX_COMPILER=$CXX"
    "-DCMAKE_INSTALL_PREFIX=$ARROW_BUILD_DIR"
    "-DARROW_BOOST_USE_SHARED=OFF"
    "-DARROW_BUILD_SHARED=OFF"
    "-DARROW_BUILD_STATIC=ON"
    "-DBOOST_SOURCE=SYSTEM"
    "-DARROW_WITH_BROTLI=OFF"
    "-DARROW_WITH_LZ4=OFF"
    "-DARROW_WITH_SNAPPY=OFF"
    "-DARROW_WITH_ZLIB=OFF"
    "-DARROW_JSON=OFF"
    "-DARROW_DATASET=OFF"
    "-DARROW_BUILD_UTILITIES=OFF"
    "-DARROW_COMPUTE=OFF"
    "-DARROW_FILESYSTEM=OFF"
    "-DARROW_USE_GLOG=OFF"
    "-DARROW_HDFS=OFF"
    "-DARROW_WITH_BACKTRACE=OFF"
    "-DARROW_JEMALLOC_USE_SHARED=OFF"
    "-DARROW_BUILD_TESTS=OFF"
    "-DBoost_INCLUDE_DIR=$DEPENDENCY_DIR/boost/include"
    "-DBOOST_SYSTEM_LIBRARY=$DEPENDENCY_DIR/boost/lib/libboost_system.a"
    "-DBOOST_FILESYSTEM_LIBRARY=$DEPENDENCY_DIR/boost/lib/libboost_filesystem.a"
    "-DBOOST_REGEX_LIBRARY=$DEPENDENCY_DIR/boost/lib/libboost_regex.a"
)

rm -rf $ARROW_BUILD_DIR
rm -rf $ARROW_DEPS_BUILD_DIR
rm -rf $ARROW_CMAKE_BUILD_DIR
mkdir $ARROW_BUILD_DIR
mkdir $ARROW_DEPS_BUILD_DIR
mkdir $ARROW_DEPS_BUILD_DIR/lib
mkdir $ARROW_CMAKE_BUILD_DIR

cd $ARROW_CMAKE_BUILD_DIR
$CMAKE -E env $CMAKE ${arrow_configure_opts[@]} -DARROW_CXXFLAGS="-O2 -m64 -fPIC -pthread" ../

make
make install

#make install does not do much here
cp -f $ARROW_CMAKE_BUILD_DIR/jemalloc_ep-prefix/src/jemalloc_ep/lib/*.a $ARROW_DEPS_BUILD_DIR/lib/

cd $DIR

echo === zip_files "arrow" "$ARROW_VERSION" "$target" "arrow arrow_deps boost"
zip_files "arrow" "$ARROW_VERSION" "$target" "arrow arrow_deps boost"

if [[ -n "$GITHUB_ACTIONS" ]]; then
    rm -rf $ARROW_SOURCE_DIR
fi
