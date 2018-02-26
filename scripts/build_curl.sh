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
PKG_CONFIG_PATH="$OPENSSL_BUILD_DIR/lib/pkgconfig" LIBS="-ldl" ./configure ${curl_configure_opts[@]}
echo "Building and Installing Curl"
make 2>&1 > /dev/null
make install 2>&1 > /dev/null

