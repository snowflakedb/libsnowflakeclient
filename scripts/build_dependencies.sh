#!/bin/bash -e
#
# Build Dependencies
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build Dependencies"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
if [ "$#" -eq 0 ]; then
    target="-t Release"
else
    target="$@"
fi
source $DIR/_init.sh $target
if [[ "$PLATFORM" == "linux" ]]; then
    source $DIR/build_uuid.sh $target
fi
source $DIR/build_zlib.sh $target
source $DIR/build_openssl.sh $target
source $DIR/build_oob.sh $target
source $DIR/build_curl.sh $target
source $DIR/build_awssdk.sh $target
source $DIR/build_arrow.sh  $target
source $DIR/build_cmocka.sh $target
source $DIR/build_azuresdk.sh $target
