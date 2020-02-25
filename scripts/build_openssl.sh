#!/bin/bash -e
#
# Build OpenSSL
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build OpenSSL"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

OPENSSL_VERSION=1.1.1b

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@

[[ -n "$GET_VERSION" ]] && echo $OPENSSL_VERSION && exit 0

OPENSSL_TAR_GZ=$DEPS_DIR/openssl-${OPENSSL_VERSION}.tar.gz
OPENSSL_SOURCE_DIR=$DEPS_DIR/openssl-${OPENSSL_VERSION}/

tar -xzf $OPENSSL_TAR_GZ -C $DEPS_DIR

# build openssl
OPENSSL_BUILD_DIR=$DEPENDENCY_DIR/openssl
rm -rf $OPENSSL_BUILD_DIR
mkdir -p $OPENSSL_BUILD_DIR

openssl_config_opts=()
openssl_config_opts+=(
    "no-shared"
    "--prefix=$OPENSSL_BUILD_DIR"
    "--openssldir=$OPENSSL_BUILD_DIR"
)
if [[ "$target" != "Release" ]]; then
    openssl_config_opts+=("--debug")
fi

cd $OPENSSL_SOURCE_DIR
if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    export CC="${GCC:-gcc52}"
    make distclean clean &> /dev/null || true
    ./Configure linux-x86_64 "${openssl_config_opts[@]}"
    make depend > /dev/null
    make -j 4 > /dev/null
    make install_sw install_ssldirs > /dev/null
elif [[ "$PLATFORM" == "darwin" ]]; then
    openssl_config_opts+=("-mmacosx-version-min=10.12")
    # Check to see if we are doing a universal build or not.
    # If we are not doing a universal build, pick an arch to
    # build
    if [[ "$UNIVERSAL" == "true" ]]; then
        # OSX/macos 32 and 64 bit universal
        echo "[INFO] Building Universal Binary"
        make distclean clean &> /dev/null || true
        ./Configure darwin-i386-cc "${openssl_config_opts[@]}"
        make -j 4 build_libs #> /dev/null
        make install_sw install_ssldirs > /dev/null
        make distclean clean &> /dev/null || true
        ./Configure darwin64-x86_64-cc "${openssl_config_opts[@]}"
        make -j 4 build_libs #> /dev/null
        lipo -create $OPENSSL_BUILD_DIR/lib/libssl.a    ./libssl.a    -output $OPENSSL_BUILD_DIR/lib/../libssl.a
        lipo -create $OPENSSL_BUILD_DIR/lib/libcrypto.a ./libcrypto.a -output $OPENSSL_BUILD_DIR/lib/../libcrypto.a
        mv $OPENSSL_BUILD_DIR/lib/../libssl.a    $OPENSSL_BUILD_DIR/lib/libssl.a
        mv $OPENSSL_BUILD_DIR/lib/../libcrypto.a $OPENSSL_BUILD_DIR/lib/libcrypto.a
    else
        make distclean clean &> /dev/null || true
        if [[ "$ARCH" == "x86" ]]; then
            echo "[INFO] Building x86 binary"
            ./Configure darwin-i386-cc "${openssl_config_opts[@]}"
            
        else
            echo "[INFO] Building x64 binary"
            ./Configure darwin64-x86_64-cc "${openssl_config_opts[@]}"
        fi
        make -j 4 > /dev/null
        make install_sw install_ssldirs > /dev/null
    fi
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi
