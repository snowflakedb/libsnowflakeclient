#!/bin/bash -e
#
# Build Curl
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build Curl"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

CURL_DIR=7.78.0
CURL_VERSION=${CURL_DIR}.1

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $CURL_VERSION && exit 0

LIBCURL_SOURCE_DIR=$DEPS_DIR/curl-${CURL_DIR}/
OOB_DEPENDENCY_DIR=$DEPENDENCY_DIR/oob
UUID_DEPENDENCY_DIR=$DEPENDENCY_DIR/uuid

# build libcurl
curl_configure_opts=()
if [[ "$target" != "Release" ]]; then
    curl_configure_opts+=("--enable-debug")
fi
LIBCURL_BUILD_DIR=$DEPENDENCY_DIR/curl
rm -rf $LIBCURL_BUILD_DIR
mkdir -p $LIBCURL_BUILD_DIR

export OPENSSL_BUILD_DIR=$DEPENDENCY_DIR/openssl
curl_configure_opts+=(
    "--with-ssl=$OPENSSL_BUILD_DIR"
    "--without-nss"
)
rm -rf $LIBCURL_BUILD_DIR
curl_configure_opts+=(
    "--enable-static"
    "--disable-shared"
    "--prefix=$LIBCURL_BUILD_DIR"
    "--without-libssh2"
    "--without-brotli"
    "--without-zstd"
    "--disable-rtsp"
    "--disable-ldap"
    "--disable-ldaps"
    "--disable-telnet"
    "--disable-tftp"
    "--disable-imap"
    "--disable-smb"
    "--disable-smtp"
    "--disable-gopher"
    "--disable-pop3"
    "--disable-ftp"
    "--disable-dict"
    "--disable-file"
    "--disable-manual"
)
cd $LIBCURL_SOURCE_DIR
echo "Building Curl with OpenSSL"
if [[ "$PLATFORM" == "linux" ]]; then
    # Linux 64 bit
    export CC="${GCC:-gcc52}"
    export CFLAGS=-pthread # required to build with gcc52 or OpenSSL check will fail
    export CPPFLAGS="-I$OOB_DEPENDENCY_DIR/include -I$UUID_DEPENDENCY_DIR/include"
    export LDFLAGS="-L$OOB_DEPENDENCY_DIR/lib -L$UUID_DEPENDENCY_DIR/lib"
    PKG_CONFIG="pkg-config -static" LIBS="-ltelemetry -luuid -ldl" /bin/sh ./configure ${curl_configure_opts[@]}
    make
    make install
elif [[ "$PLATFORM" == "darwin" ]]; then
    # Check to see if we are doing a universal build or not.
    # If we are not doing a universal build, pick an arch to
    # build
    if [[ "$ARCH" == "universal" ]]; then
        echo "[INFO] Building Universal Binary"
        make distclean clean &> /dev/null || true
        export CFLAGS="-arch x86_64 -Xarch_x86_64 -DSIZEOF_LONG_INT=8 -mmacosx-version-min=${MACOSX_VERSION_MIN}"
        export CPPFLAGS=-I$OOB_DEPENDENCY_DIR/include
        export LDFLAGS=-L$OOB_DEPENDENCY_DIR/lib
        PKG_CONFIG="pkg-config -static" LIBS="-ltelemetry -ldl" ./configure ${curl_configure_opts[@]}
        make > /dev/null
        make install /dev/null

        make distclean clean &> /dev/null || true
        export CFLAGS="-arch i386 -Xarch_i386 -DSIZEOF_LONG_INT=4 -Xarch_i386 -DHAVE_LONG_LONG -mmacosx-version-min=${MACOSX_VERSION_MIN}"
        export CPPFLAGS=-I$OOB_DEPENDENCY_DIR/include
        export LDFLAGS=-L$OOB_DEPENDENCY_DIR/lib
        PKG_CONFIG="pkg-config -static" LIBS="-ltelemetry -ldl" ./configure ${curl_configure_opts[@]}
        make > /dev/null
        echo "lipo -create $LIBCURL_BUILD_DIR/lib/libcurl.a ./lib/.libs/libcurl.a -output $LIBCURL_BUILD_DIR/lib/../libcurl.a"
        lipo -create $LIBCURL_BUILD_DIR/lib/libcurl.a ./lib/.libs/libcurl.a -output $LIBCURL_BUILD_DIR/lib/../libcurl.a
        mv $LIBCURL_BUILD_DIR/lib/../libcurl.a $LIBCURL_BUILD_DIR/lib/libcurl.a
    elif [[ "$ARCH" == "x86" ]]; then
        echo "[INFO] Building x86 Binary"
        make distclean clean &> /dev/null || true
        export CFLAGS="-arch i386 -Xarch_i386 -DSIZEOF_LONG_INT=4 -Xarch_i386 -DHAVE_LONG_LONG -mmacosx-version-min=${MACOSX_VERSION_MIN}"
        export CPPFLAGS=-I$OOB_DEPENDENCY_DIR/include
        export LDFLAGS=-L$OOB_DEPENDENCY_DIR/lib
        PKG_CONFIG="pkg-config -static" LIBS="-ltelemetry -ldl" ./configure ${curl_configure_opts[@]}
        make > /dev/null
        make install /dev/null
    else
        echo "[INFO] Building x64 Binary"
        make distclean clean &> /dev/null || true
        export CFLAGS="-arch x86_64 -Xarch_x86_64 -DSIZEOF_LONG_INT=8 -mmacosx-version-min=${MACOSX_VERSION_MIN}"
        export CPPFLAGS=-I$OOB_DEPENDENCY_DIR/include
        export LDFLAGS=-L$OOB_DEPENDENCY_DIR/lib
        PKG_CONFIG="pkg-config -static" LIBS="-ltelemetry -ldl" ./configure ${curl_configure_opts[@]}
        make > /dev/null
        make install /dev/null
    fi
fi
echo === zip_file "curl" "$CURL_VERSION" "$target"
zip_file "curl" "$CURL_VERSION" "$target"
