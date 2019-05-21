#!/bin/bash -e
#
# Build Curl
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build Curl"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh
LIBCURL_SOURCE_DIR=$DEPS_DIR/curl-7.58.0/

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
    PKG_CONFIG="pkg-config -static" LIBS="-ldl" ./configure ${curl_configure_opts[@]}
    make > /dev/null
    make install> /dev/null
elif [[ "$PLATFORM" == "darwin" ]]; then
    make distclean clean &> /dev/null || true
    export CFLAGS="-arch x86_64 -Xarch_x86_64 -DSIZEOF_LONG_INT=8 -mmacosx-version-min=10.12"
    PKG_CONFIG="pkg-config -static" LIBS="-ldl" ./configure ${curl_configure_opts[@]}
    make > /dev/null
    make install /dev/null

    make distclean clean &> /dev/null || true
    export CFLAGS="-arch i386 -Xarch_i386 -DSIZEOF_LONG_INT=4 -Xarch_i386 -DHAVE_LONG_LONG -mmacosx-version-min=10.12"
    PKG_CONFIG="pkg-config -static" LIBS="-ldl" ./configure ${curl_configure_opts[@]}
    make > /dev/null
    echo "lipo -create $LIBCURL_BUILD_DIR/lib/libcurl.a ./lib/.libs/libcurl.a -output $LIBCURL_BUILD_DIR/lib/../libcurl.a"
    lipo -create $LIBCURL_BUILD_DIR/lib/libcurl.a ./lib/.libs/libcurl.a -output $LIBCURL_BUILD_DIR/lib/../libcurl.a
    mv $LIBCURL_BUILD_DIR/lib/../libcurl.a $LIBCURL_BUILD_DIR/lib/libcurl.a
fi
