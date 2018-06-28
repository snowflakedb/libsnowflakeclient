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
if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    export CC=gcc52
    make distclean clean &> /dev/null || true
    ./Configure linux-x86_64 "${openssl_config_opts[@]}"
    make depend > /dev/null
    make -j 4 > /dev/null
    make install_sw install_ssldirs > /dev/null
elif [[ "$PLATFORM" == "darwin" ]]; then
    # OSX/macos 32 and 64 bit universal
    openssl_config_opts+=("-mmacosx-version-min=10.11")
    make distclean clean &> /dev/null || true
    ./Configure darwin-i386-cc "${openssl_config_opts[@]}"
    make -j 4 > /dev/null
    make install_sw install_ssldirs > /dev/null
    make distclean clean &> /dev/null || true
    ./Configure darwin64-x86_64-cc "${openssl_config_opts[@]}"
    make -j 4 > /dev/null
    lipo -create $OPENSSL_BUILD_DIR/lib/libssl.a    ./libssl.a    -output $OPENSSL_BUILD_DIR/lib/../libssl.a
    lipo -create $OPENSSL_BUILD_DIR/lib/libcrypto.a ./libcrypto.a -output $OPENSSL_BUILD_DIR/lib/../libcrypto.a
    mv $OPENSSL_BUILD_DIR/lib/../libssl.a    $OPENSSL_BUILD_DIR/lib/libssl.a
    mv $OPENSSL_BUILD_DIR/lib/../libcrypto.a $OPENSSL_BUILD_DIR/lib/libcrypto.a
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi
