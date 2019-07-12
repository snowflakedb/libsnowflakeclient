#!/bin/bash -e
#
# Build Snowflake Client library
#
function usage() {
    echo "Usage: `basename $0` [-p]"
    echo "-p                 : Rebuild Snowflake Client with profile option. default: no profile"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-s                 : Build source only. Skipping building tests."
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@

cd $DIR/..
#Symlink wont work on Appveyor Windows.
rm -f tests/test_simple_put_azure.cpp
cp -f tests/test_simple_put.cpp tests/test_simple_put_azure.cpp

rm -rf cmake-build
mkdir cmake-build
cd cmake-build
cmake_opts=(
    "-DCMAKE_C_COMPILER=$GCC"
    "-DCMAKE_CXX_COMPILER=$GXX"
    "-DCMAKE_BUILD_TYPE=$target"
)

if [[ "$PLATFORM" == "darwin" ]]; then
    cmake_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;i386")
fi

if [[ "$BUILD_SOURCE_ONLY" == "true" ]]; then
    cmake_opts+=("-DBUILD_TESTS=OFF")
fi

if [[ "$ENABLE_MOCK_OBJECTS" == "true" ]]; then
    cmake_opts+=("-DMOCK=ON")
fi

$CMAKE ${cmake_opts[@]} ..
make
