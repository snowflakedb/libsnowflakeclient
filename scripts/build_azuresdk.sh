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

AZURE_VERSION=0.1.18

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

AZURE_SOURCE_DIR=$DEPS_DIR/azure-storage-cpplite
AZURE_BUILD_DIR=$DEPENDENCY_DIR/azure
AZURE_CMAKE_BUILD_DIR=$AZURE_SOURCE_DIR/cmake-build

GIT_REPO="https://github.com/snowflakedb/azure-storage-cpplite.git"
CLONE_CMD="git clone -b master $GIT_REPO $AZURE_SOURCE_DIR"

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
  git checkout tags/v$AZURE_VERSION -b v$AZURE_VERSION || true
else
  cd $AZURE_SOURCE_DIR
  git fetch || true
  git checkout tags/v$AZURE_VERSION -b v$AZURE_VERSION || true
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
    "-DCURL_INCLUDE_DIRS=$DEPENDENCY_DIR/curl/include"
    "-DCURL_LIBRARIES=$DEPENDENCY_DIR/curl/lib/libcurl.a"
    "-DEXTRA_INCLUDE=$DEPENDENCY_DIR/zlib/include"
)

if [[ "$PLATFORM" == "linux" ]]; then
  azure_configure_opts+=(
      "-DUUID_INCLUDE_DIR=$DEPENDENCY_DIR/uuid/include"
      "-DUUID_LIBRARIES=$DEPENDENCY_DIR/uuid/lib/libuuid.a"
  )
fi

ADDITIONAL_CXXFLAGS=
# Check to see if we are doing a universal build or not.
# If we are not doing a universal build, pick an arch to
# build
if [[ "$PLATFORM" == "darwin" ]]; then
  if [[ "$ARCH" == "universal" ]]; then
    echo "[INFO] Building Universal Binary"
    azure_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;i386")
  elif [[ "$ARCH" == "x86" ]]; then
    echo "[INFO] Building x86 Binary"
    azure_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=i386")
  else
    echo "[INFO] Building x64 Binary"
    azure_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64")
  fi
  ADDITIONAL_CXXFLAGS="-mmacosx-version-min=${MACOSX_VERSION_MIN} "
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
  $CMAKE -E env $CMAKE ${azure_configure_opts[@]} -DEXTRA_LIBRARIES="-lrt -ldl -pthread $DEPENDENCY_DIR/zlib/lib/libz.a $DEPENDENCY_DIR/oob/lib/libtelemetry.a" ../
else
  $CMAKE -E env $CMAKE ${azure_configure_opts[@]} CXXFLAGS=$ADDITIONAL_CXXFLAGS LDFLAGS=$ADDITIONAL_CXXFLAGS -DEXTRA_LIBRARIES="-framework Foundation -framework SystemConfiguration -ldl -lpthread $DEPENDENCY_DIR/zlib/lib/libz.a $DEPENDENCY_DIR/oob/lib/libtelemetry.a" ../
fi

unset GIT_DIR
    
make
make install

#make install does not do much here
cp -fr $AZURE_SOURCE_DIR/include $DEPENDENCY_DIR/azure/
mkdir -p $DEPENDENCY_DIR/azure/lib
cp -fr $AZURE_CMAKE_BUILD_DIR/libazure-storage-lite.a $DEPENDENCY_DIR/azure/lib/

cd $DIR

echo === zip_file "azure" "$AZURE_VERSION" "$target"
zip_file "azure" "$AZURE_VERSION" "$target"
