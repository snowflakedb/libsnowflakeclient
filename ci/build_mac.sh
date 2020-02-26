#!/bin/bash -e
#
# Build libsnowflakeclient for Mac
#
set -o pipefail

CI_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
$CI_DIR/build/build.sh
