#!/bin/bash -e
#
# build azure-cpp-lite
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build AZURE SDK"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

export CC="/usr/lib64/ccache/gcc52 -g"
export CXX="/usr/lib64/ccache/g++52 -g"

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
AZURE_SOURCE_DIR=$DEPS_DIR/azure-storage-cpplite
AZURE_BUILD_DIR=$DEPENDENCY_DIR/azure
AZURE_CMAKE_BUILD_DIR=$AZURE_SOURCE_DIR/cmake-build

GIT_REPO="https://github.com/snowflakedb/azure-storage-cpplite.git"
CLONE_CMD="git clone -b master $GIT_REPO $AZURE_SOURCE_DIR"
VERSION="v0.1.6"

if [ ! -d $AZURE_SOURCE_DIR ]; then
  n=0 
  # retry 5 times on cloning
  until [ $n -ge 5 ] 
  do
    if $CLONE_CMD ; then
      break
    fi  
    n=$[$n+1]
  done
  
  if [ ! -d $AZURE_SOURCE_DIR ]; then
    echo "[Error] failed to clone repo from $GIT_REPO"
    exit 1
  fi  

  cd $AZURE_SOURCE_DIR
  git checkout tags/$VERSION -b $VERSION || true
else
  cd $AZURE_SOURCE_DIR
  git fetch
  git checkout tags/$VERSION -b $VERSION || true
fi


azure_configure_opts=()
if [[ "$target" != "Release" ]]; then
    azure_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
    AZURE_CMAKE_BUILD_DIR=$AZURE_SOURCE_DIR/cmake-build-debug
else
    azure_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
azure_configure_opts+=(
    "-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"
    "-DCMAKE_C_COMPILER=$CC"
    "-DCMAKE_CXX_COMPILER=$CXX"
    "-DCMAKE_INSTALL_PREFIX=$AZURE_BUILD_DIR"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DUSE_OPENSSL=true"
    "-DOPENSSL_VERSION_NUMBER=0x11100000L"
    "-DBUILD_SAMPLES=true"
    "-DBUILD_TESTS=true"
    "-DOPENSSL_INCLUDE_DIR=$DEPENDENCY_DIR/openssl/include"
    "-DOPENSSL_CRYPTO_LIBRARY=$DEPENDENCY_DIR/openssl/lib/libcrypto.a"
    "-DOPENSSL_SSL_LIBRARY=$DEPENDENCY_DIR/openssl/lib/libssl.a"
    "-DUUID_INCLUDE_DIR=$DEPENDENCY_DIR/uuid/include"
    "-DUUID_LIBRARIES=$DEPENDENCY_DIR/uuid/lib/libuuid.a"
    "-DCURL_INCLUDE_DIRS=$DEPENDENCY_DIR/curl/include"
    "-DCURL_LIBRARIES=$DEPENDENCY_DIR/curl/lib/libcurl.a"
    "-DEXTRA_INCLUDE=$DEPENDENCY_DIR/zlib/include"
)


ADDITIONAL_CXXFLAGS=
if [[ "$PLATFORM" == "darwin" ]]; then
    azure_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;i386")
    ADDITIONAL_CXXFLAGS="-mmacosx-version-min=10.12 "
fi

rm -rf $AZURE_BUILD_DIR
rm -rf $AZURE_CMAKE_BUILD_DIR
mkdir $AZURE_BUILD_DIR
mkdir $AZURE_CMAKE_BUILD_DIR
export CMAKE_CXX_FLAGS=$ADDITIONAL_CXXFLAGS
export LDFLAGS+=$ADDITIONAL_CXXFLAGS

export GIT_DIR=/tmp

cd $AZURE_CMAKE_BUILD_DIR
if [ "$(uname -s)" == "Linux" ] ; then
  $CMAKE -E env $CMAKE ${azure_configure_opts[@]} -DEXTRA_LIBRARIES="-lrt -ldl -pthread $DEPENDENCY_DIR/zlib/lib/libz.a" ../
else
  $CMAKE -E env $CMAKE ${azure_configure_opts[@]} CXXFLAGS=$ADDITIONAL_CXXFLAGS LDFLAGS=$ADDITIONAL_CXXFLAGS -DEXTRA_LIBRARIES="-ldl -lpthread $DEPENDENCY_DIR/zlib/lib/libz.a" ../
fi
    
make
make install

#make install does not do much here
cp -fr $AZURE_SOURCE_DIR/include $DEPENDENCY_DIR/azure/
mkdir -p $DEPENDENCY_DIR/azure/lib
cp -fr $AZURE_CMAKE_BUILD_DIR/libazure-storage-lite.a $DEPENDENCY_DIR/azure/lib/
