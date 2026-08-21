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

OPENSSL_SRC_VERSION=3.5.7
OPENSSL_BUILD_VERSION=2
OPENSSL_VERSION=$OPENSSL_SRC_VERSION.$OPENSSL_BUILD_VERSION

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $OPENSSL_VERSION && exit 0

OPENSSL_SOURCE_DIR=$DEPS_DIR/openssl/

rm -rf $OPENSSL_SOURCE_DIR
git clone --single-branch --branch openssl-$OPENSSL_SRC_VERSION --recursive https://github.com/openssl/openssl.git $OPENSSL_SOURCE_DIR

# build openssl
OPENSSL_BUILD_DIR=$DEPENDENCY_DIR/openssl
rm -rf $OPENSSL_BUILD_DIR
mkdir -p $OPENSSL_BUILD_DIR

# SNOW-3649681: configure OpenSSL with standard root-owned --prefix/--openssldir
# (so the build directory is not compiled into the library), and redirect only
# the install destinations back into the build tree via the INSTALLTOP/OPENSSLDIR
# overrides below so the staged artifact layout is unchanged. See SNOW-3649681.
OPENSSL_RUNTIME_PREFIX=/usr/local
OPENSSL_RUNTIME_SSLDIR=/usr/local/ssl

openssl_config_opts=()
openssl_config_opts+=(
    "no-shared"
    "enable-fips"
    "--prefix=$OPENSSL_RUNTIME_PREFIX"
    "--openssldir=$OPENSSL_RUNTIME_SSLDIR"
    "--libdir=lib"
)
if [[ "$target" != "Release" ]]; then
    openssl_config_opts+=("--debug")
fi

# Override the install destinations (not the compiled-in paths) so the build
# artifacts are still staged under $OPENSSL_BUILD_DIR exactly as before. These
# make variables only control where "make install_*" copies files; the
# OPENSSLDIR/ENGINESDIR/MODULESDIR values baked into the libraries during the
# preceding "make" keep the safe defaults configured above.
openssl_install_opts=(
    "INSTALLTOP=$OPENSSL_BUILD_DIR"
    "OPENSSLDIR=$OPENSSL_BUILD_DIR"
)

cd $OPENSSL_SOURCE_DIR
if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    make distclean clean &> /dev/null || true
    perl ./Configure linux-$(uname -m) "${openssl_config_opts[@]}"
    make depend > /dev/null
    make -j 4 > /dev/null
    make install_sw install_ssldirs install_fips "${openssl_install_opts[@]}" > /dev/null
    # get FIPS module from verified version
    cp $DEPENDENCY_DIR/openssl_fips/lib/ossl-modules/fips.so $OPENSSL_BUILD_DIR/lib/ossl-modules/fips.so
elif [[ "$PLATFORM" == "darwin" ]]; then
    openssl_config_opts+=("-mmacosx-version-min=${MACOSX_VERSION_MIN}")
    # Check to see if we are doing a universal build or not.
    # If we are not doing a universal build, pick an arch to
    # build
    if [[ "$ARCH" == "universal" ]]; then
        # OSX/macos x64 and arm64 bit universal
        echo "[INFO] Building Universal Binary"
        make distclean clean &> /dev/null || true
        perl ./Configure darwin64-arm64-cc "${openssl_config_opts[@]}"
        make -j 4 build_libs > /dev/null
        make install_sw install_ssldirs install_fips "${openssl_install_opts[@]}" > /dev/null
        mv $OPENSSL_BUILD_DIR/lib $OPENSSL_BUILD_DIR/libarm64
        make distclean clean &> /dev/null || true
        perl ./Configure darwin64-x86_64-cc "${openssl_config_opts[@]}"
        # Build the libraries (which bake in OPENSSLDIR/ENGINESDIR/MODULESDIR)
        # with the safe configured paths *before* the install step applies the
        # build-dir overrides, so the x86_64 slice gets the same hardened paths
        # as the arm64 slice above.
        make -j 4 build_libs > /dev/null
        make install_sw install_ssldirs install_fips "${openssl_install_opts[@]}" > /dev/null
        lipo -create $OPENSSL_BUILD_DIR/lib/libssl.a    $OPENSSL_BUILD_DIR/libarm64/libssl.a    -output $OPENSSL_BUILD_DIR/lib/../libssl.a
        lipo -create $OPENSSL_BUILD_DIR/lib/libcrypto.a $OPENSSL_BUILD_DIR/libarm64/libcrypto.a -output $OPENSSL_BUILD_DIR/lib/../libcrypto.a
        lipo -create $OPENSSL_BUILD_DIR/lib/ossl-modules/legacy.dylib $OPENSSL_BUILD_DIR/libarm64/ossl-modules/legacy.dylib -output $OPENSSL_BUILD_DIR/lib/../legacy.dylib
        mv $OPENSSL_BUILD_DIR/lib/../libssl.a    $OPENSSL_BUILD_DIR/lib/libssl.a
        mv $OPENSSL_BUILD_DIR/lib/../libcrypto.a $OPENSSL_BUILD_DIR/lib/libcrypto.a
        mv $OPENSSL_BUILD_DIR/lib/../legacy.dylib $OPENSSL_BUILD_DIR/lib/ossl-modules/legacy.dylib
        rm -rf $OPENSSL_BUILD_DIR/libarm64
        lipo -info $OPENSSL_BUILD_DIR/lib/libssl.a
        lipo -info $OPENSSL_BUILD_DIR/lib/libcrypto.a
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
        make install_sw install_ssldirs install_fips "${openssl_install_opts[@]}" > /dev/null
    fi
    # get FIPS module from verified version
    cp $DEPENDENCY_DIR/openssl_fips/lib/ossl-modules/fips.dylib $OPENSSL_BUILD_DIR/lib/ossl-modules/fips.dylib
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi


# openssl3 changed lib path to lib64 for 64-bit build, move it back to keep the compatibility
if [[ ! -d "$OPENSSL_BUILD_DIR/lib" ]] && [[ -d "$OPENSSL_BUILD_DIR/lib64" ]]; then
    mv $OPENSSL_BUILD_DIR/lib64 $OPENSSL_BUILD_DIR/lib
fi

# SNOW-3649681 configured OpenSSL with --prefix=$OPENSSL_RUNTIME_PREFIX so the
# build directory is not compiled into libcrypto/libssl. That same prefix is,
# however, baked into the generated pkg-config (.pc) files, whose absolute
# prefix/libdir/includedir are NOT relocated by the INSTALLTOP install override.
# curl's ./configure discovers OpenSSL via pkg-config, so a stale
# "$OPENSSL_RUNTIME_PREFIX/lib" makes it emit -L$OPENSSL_RUNTIME_PREFIX/lib,
# fail to find libcrypto in the staged tree, and abort with "OpenSSL could not
# be detected". Repoint the .pc metadata at the real staged tree. (The CMake
# config files compute their prefix relative to their own location, so they are
# already relocatable and need no fixup.) These are build-time inputs only (not
# shipped), so the hardened paths compiled into the libraries are unchanged.
if compgen -G "$OPENSSL_BUILD_DIR/lib/pkgconfig/*.pc" > /dev/null; then
    perl -pi \
        -e "s{^prefix=\Q$OPENSSL_RUNTIME_PREFIX\E(?=/|\$)}{prefix=$OPENSSL_BUILD_DIR};" \
        -e "s{^libdir=\Q$OPENSSL_RUNTIME_PREFIX\E/}{libdir=$OPENSSL_BUILD_DIR/};" \
        -e "s{^includedir=\Q$OPENSSL_RUNTIME_PREFIX\E/}{includedir=$OPENSSL_BUILD_DIR/};" \
        "$OPENSSL_BUILD_DIR"/lib/pkgconfig/*.pc
fi

echo === zip_file "openssl" "$OPENSSL_VERSION" "$target"
zip_file "openssl" "$OPENSSL_VERSION" "$target"
