#!/bin/bash -e
#
# Build libsnowflakeclient for Mac
#
set -o pipefail

env | sort
CI_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_CMD=("$CI_DIR/build/build.sh")
# _init.sh clears BUILD_SOURCE_ONLY unless -s is passed
if [[ "$BUILD_SOURCE_ONLY" == "true" ]]; then
    BUILD_CMD+=(-s)
fi
"${BUILD_CMD[@]}"
