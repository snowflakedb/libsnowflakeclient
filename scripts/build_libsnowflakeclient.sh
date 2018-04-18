#!/bin/bash -e
#
# Build Snowflake Client library
#
function usage() {
    echo "Usage: `basename $0` [-p]"
    echo "-p                 : Rebuild Snowflake Client with profile option. default: no profile"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh

cd $DIR/..
rm -rf cmake-build
mkdir cmake-build
cd cmake-build
cmake_opts=(
    "-DCMAKE_C_COMPILER=$GCC"
    "-DCMAKE_CXX_COMPILER=$GXX"
    "-DCMAKE_BUILD_TYPE=$target"
)
$CMAKE ${cmake_opts[@]} ..
make
