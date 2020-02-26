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

# Check to see if we are doing a universal build or not.
# If we are not doing a universal build, pick an arch to
# build
if [[ "$PLATFORM" == "darwin" ]]; then
    if [[ "$UNIVERSAL" == "true" ]]; then
        echo "[INFO] Building Universal Binary"
        cmake_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;i386")
    else
        if [[ "$ARCH" == "x86" ]]; then
            echo "[INFO] Building x86 Binary"
            cmake_opts+=("-DCMAKE_OSX_ARCHITECTURES=i386")
        else
            echo "[INFO] Building x64 Binary"
            cmake_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64")
        fi
    fi
fi

if [[ "$BUILD_SOURCE_ONLY" == "true" ]]; then
    cmake_opts+=("-DBUILD_TESTS=OFF")
fi

if [[ "$ENABLE_MOCK_OBJECTS" == "true" ]]; then
    cmake_opts+=("-DMOCK=ON")
fi

$CMAKE ${cmake_opts[@]} ..
make

