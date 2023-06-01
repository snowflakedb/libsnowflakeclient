#!/bin/bash -e
#
# Build OpenSSL
# GitHub repo: https://github.com/openssl/openssl.git
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build OpenSSL"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

OPENSSL_VERSION=3.0.8

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $OPENSSL_VERSION && exit 0

OPENSSL_SOURCE_DIR=$DEPS_DIR/openssl-${OPENSSL_VERSION}/

# build openssl
OPENSSL_BUILD_DIR=$DEPENDENCY_DIR/openssl
rm -rf $OPENSSL_BUILD_DIR
mkdir -p $OPENSSL_BUILD_DIR

openssl_config_opts=()
openssl_config_opts+=(
    "no-shared"
    "enable-fips"
    "--prefix=$OPENSSL_BUILD_DIR"
    "--openssldir=$OPENSSL_BUILD_DIR"
)
if [[ "$target" != "Release" ]]; then
    openssl_config_opts+=("--debug")
fi

cd $OPENSSL_SOURCE_DIR
if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    # this should be made in docker image just around it for now
    if [[ "x86_64" == $(uname -p) ]]; then
        yum install perl-IPC-Cmd
    fi
    make distclean clean &> /dev/null || true
    perl ./Configure linux-$(uname -p) "${openssl_config_opts[@]}"
    make depend > /dev/null
    make -j 4 > /dev/null
    make install_sw install_ssldirs install_fips > /dev/null
elif [[ "$PLATFORM" == "darwin" ]]; then
    openssl_config_opts+=("-mmacosx-version-min=${MACOSX_VERSION_MIN}")
    # Check to see if we are doing a universal build or not.
    # If we are not doing a universal build, pick an arch to
    # build
    if [[ "$ARCH" == "universal" ]]; then
        # OSX/macos 32 and 64 bit universal
        echo "[INFO] Building Universal Binary"
        make distclean clean &> /dev/null || true
        perl ./Configure darwin-i386-cc "${openssl_config_opts[@]}"
        make -j 4 build_libs > /dev/null
        make install_sw install_ssldirs install_fips > /dev/null
        make distclean clean &> /dev/null || true
        perl ./Configure darwin64-x86_64-cc "${openssl_config_opts[@]}"
        make -j 4 build_libs > /dev/null
        lipo -create $OPENSSL_BUILD_DIR/lib/libssl.a    ./libssl.a    -output $OPENSSL_BUILD_DIR/lib/../libssl.a
        lipo -create $OPENSSL_BUILD_DIR/lib/libcrypto.a ./libcrypto.a -output $OPENSSL_BUILD_DIR/lib/../libcrypto.a
        lipo -create $OPENSSL_BUILD_DIR/lib/ossl-modules/fips.dylib ./providers/fips.dylib -output $OPENSSL_BUILD_DIR/lib/../fips.dylib
        lipo -create $OPENSSL_BUILD_DIR/lib/ossl-modules/legacy.dylib ./providers/legacy.dylib -output $OPENSSL_BUILD_DIR/lib/../legacy.dylib
        mv $OPENSSL_BUILD_DIR/lib/../libssl.a    $OPENSSL_BUILD_DIR/lib/libssl.a
        mv $OPENSSL_BUILD_DIR/lib/../libcrypto.a $OPENSSL_BUILD_DIR/lib/libcrypto.a
        mv $OPENSSL_BUILD_DIR/lib/../fips.dylib $OPENSSL_BUILD_DIR/lib/ossl-modules/fips.dylib
        mv $OPENSSL_BUILD_DIR/lib/../legacy.dylib $OPENSSL_BUILD_DIR/lib/ossl-modules/legacy.dylib
        lipo -info $OPENSSL_BUILD_DIR/lib/libssl.a
        lipo -info $OPENSSL_BUILD_DIR/lib/libcrypto.a
        lipo -info $OPENSSL_BUILD_DIR/lib/ossl-modules/fips.dylib
        lipo -info $OPENSSL_BUILD_DIR/lib/ossl-modules/legacy.dylib
    else
        make distclean clean &> /dev/null || true
        if [[ "$ARCH" == "x86" ]]; then
            echo "[INFO] Building x86 binary"
            perl ./Configure darwin-i386-cc "${openssl_config_opts[@]}"
            
        elif [[ "$ARCH" == "x64" ]]; then
            echo "[INFO] Building x64 binary"
            perl ./Configure darwin64-x86_64-cc "${openssl_config_opts[@]}"
        else
            echo "[INFO] Building $ARCH binary"
            perl ./Configure darwin64-$ARCH-cc "${openssl_config_opts[@]}"
        fi
        make -j 4 > /dev/null
        make install_sw install_ssldirs install_fips > /dev/null
    fi
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi

# openssl3 changed lib path to lib64 for 64-bit build, move it back to keep the compatibility
if [[ ! -d "$OPENSSL_BUILD_DIR/lib" ]] && [[ -d "$OPENSSL_BUILD_DIR/lib64" ]]; then
    mv $OPENSSL_BUILD_DIR/lib64 $OPENSSL_BUILD_DIR/lib
fi

echo === zip_file "openssl" "$OPENSSL_VERSION" "$target"
zip_file "openssl" "$OPENSSL_VERSION" "$target"
