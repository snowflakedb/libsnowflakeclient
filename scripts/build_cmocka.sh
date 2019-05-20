#!/bin/bash -e
#
# Build cmocka
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build cmocka"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}

set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh
SOURCE_DIR=$DEPS_DIR/cmocka-1.1.1

INSTALL_DIR=/tmp/cmocka
rm -rf $INSTALL_DIR
mkdir -p $INSTALL_DIR

config_opts=(
    "-DUNIT_TESTING=ON"
    "-DCMAKE_BUILD_TYPE=$target"
    "-DCMAKE_INSTALL_PREFIX=$INSTALL_DIR"
)

ADDITIONAL_CXXFLAGS=
if [[ "$PLATFORM" == "darwin" ]]; then
    config_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;i386")
    ADDITIONAL_CFLAGS="-mmacosx-version-min=10.12"
fi

cd $SOURCE_DIR
rm -rf cmake-build
mkdir cmake-build
cd cmake-build
echo cmake ${config_opts[@]} ..
$CMAKE -E env CFLAGS=$ADDITIONAL_CFLAGS $CMAKE ${config_opts[@]} ..
make
make test
make install

DEPENDENCY_DIR=$DIR/../deps-build/$PLATFORM
rm -rf $DEPENDENCY_DIR/cmocka
mkdir -p $DEPENDENCY_DIR/cmocka/{include,lib}

cd $INSTALL_DIR
cp -p -r * $DEPENDENCY_DIR/cmocka/
