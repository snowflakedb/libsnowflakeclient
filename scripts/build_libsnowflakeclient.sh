#!/bin/bash -e
#
# Build Snowflake Client library
#
function usage() {
    echo "Usage: `basename $0` [-p]"
    echo "-p                 : Rebuild Snowflake Client with profile option. default: no profile"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-s                 : Build source only. Skipping building tests."
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

LIBSNOWFLAKECLIENT_VERSION=$(grep SF_API_VERSION include/snowflake/version.h | awk '{print $3}' | sed -e 's/^"//' -e 's/"$//')

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh $@

[[ -n "$GET_VERSION" ]] && echo $LIBSNOWFLAKECLIENT_VERSION && exit 0

cd $DIR/..
#Symlink wont work on Appveyor Windows.
rm -f tests/test_simple_put_azure.cpp
cp -f tests/test_simple_put.cpp tests/test_simple_put_azure.cpp

CMAKE_DIR=cmake-build-$target
rm -rf $CMAKE_DIR
mkdir $CMAKE_DIR
cd $CMAKE_DIR
cmake_opts=(
    "-DCMAKE_C_COMPILER=$GCC"
    "-DCMAKE_CXX_COMPILER=$GXX"
    "-DCMAKE_BUILD_TYPE=$target"
)

# Check to see if we are doing a universal build or not.
# If we are not doing a universal build, pick an arch to
# build
if [[ "$PLATFORM" == "darwin" ]]; then
    cmake_opts+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_VERSION_MIN}")
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

BUILD_DIR=$DEPENDENCY_DIR/libsnowflakeclient
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/{include,lib}
echo "OK"
set -x
cp -pfr $DIR/../include/snowflake $BUILD_DIR/include
cp -p $DIR/../$CMAKE_DIR/libsnowflakeclient.a $BUILD_DIR/lib

echo === zip_file "libsnowflakeclient" "$LIBSNOWFLAKECLIENT_VERSION" "$target"
zip_file "libsnowflakeclient" "$LIBSNOWFLAKECLIENT_VERSION" "$target"
cmake_file_name=$(get_cmake_file_name "libsnowflakeclient" "$LIBSNOWFLAKE_VERSION" "$target")
if [[ -z "$GITHUB_ACTIONS" ]] && [[ -z "$BUILD_SOURCE_ONLY" ]]; then
    pushd $DIR/.. >&/dev/null
        tar cvfz artifacts/$cmake_file_name $CMAKE_DIR
    popd >&/dev/null
fi
