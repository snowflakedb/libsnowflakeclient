#!/bin/bash -e
#
# Build OpenSSL
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build OpenSSL"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh
OPENSSL_SOURCE_DIR=$DEPS_DIR/openssl-1.1.0g/

# build openssl
OPENSSL_BUILD_DIR=$DEPENDENCY_DIR/openssl
rm -rf $OPENSSL_BUILD_DIR
mkdir -p $OPENSSL_BUILD_DIR

openssl_config_opts=()
openssl_config_opts+=(
    "no-shared"
    "--prefix=$OPENSSL_BUILD_DIR"
)
if [[ "$target" != "Release" ]]; then
    openssl_config_opts+=("-d")
fi
cd $OPENSSL_SOURCE_DIR
./config ${openssl_config_opts[@]}
make depend
echo "Building and Installing OpenSSL"
make -j 8 2>&1 > /dev/null
make install_sw install_ssldirs 2>&1 > /dev/null
