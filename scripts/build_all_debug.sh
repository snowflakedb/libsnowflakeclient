#!/bin/bash -e
#
# Build Dependencies
#

SFLC_DIR="$( dirname $( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd ) )"
set -e
rm -rf $SFLC_DIR/deps-build/darwin
rm -rf $SFLC_DIR/deps/
git checkout $SFLC_DIR/deps/
 
for x in arrow cmocka zlib openssl oob uuid curl awssdk azuresdk libsnowflakeclient ; do
    echo Building $x...
    ARROW_FROM_SOURCE=1 ARCH=arm64 $SFLC_DIR/scripts/build_$x.sh -t Debug;
done
