#!/bin/bash -e
#
# Run test cases
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Run test cases"
    echo "-t <Release/Debug> : Test with Release or Debug builds"
    exit 2
}

set -o pipefail
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
target=Debug
source $DIR/_init.sh $@
source $DIR/env.sh

pushd $DIR/../cmake-build-$target
    $CTEST -V -E "valgrind.*"
popd

