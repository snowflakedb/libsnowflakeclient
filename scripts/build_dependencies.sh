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
target=Debug
source $DIR/_init.sh $@
source $DIR/build_openssl.sh -t $target
source $DIR/build_curl.sh    -t $target
source $DIR/build_awssdk.sh  -t $target
source $DIR/build_cmocka.sh  -t Debug
if [[ "$PLATFORM" == "linux" ]]; then
	source $DIR/build_uuid.sh -t $target
fi
source $DIR/build_azuresdk.sh -t $target
