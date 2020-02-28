#!/bin/bash -e
#
# Test libsnowflakeclient for Mac
#
set -o pipefail
CI_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
$CI_DIR/test/test.sh
