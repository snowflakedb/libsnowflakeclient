

#!/bin/bash -e
#
# Build libuuid
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Builds libuuid"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}

set -o pipefail
UUID_SRC_VERSION=2.39.0
UUID_BUILD_VERSION=5
UUID_VERSION=$UUID_SRC_VERSION.$UUID_BUILD_VERSION

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh
[[ -n "$GET_VERSION" ]] && echo $UUID_VERSION && exit 0

# build
export UUID_SOURCE_DIR=$DEPS_DIR/uuid

cd $UUID_SOURCE_DIR

BUILD_DIR=$DEPENDENCY_DIR/uuid
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/{lib,include/uuid}

if [[ "$PLATFORM" == "linux" ]]; then
    echo "Building uuid"
    export CFLAGS="-fPIC -DHAVE_SYS_FILE_H -DHAVE_USLEEP"
    make
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi

cp $UUID_SOURCE_DIR/libuuid/src/uuid.h $BUILD_DIR/include/uuid
cp $UUID_SOURCE_DIR/libuuid.a $BUILD_DIR/lib

echo === zip_file "uuid" "$UUID_VERSION" "$target"
zip_file "uuid" "$UUID_VERSION" "$target"
