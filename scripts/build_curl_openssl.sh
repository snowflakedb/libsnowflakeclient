#!/bin/bash -e
#
# Build Curl and OpenSSL
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Builds Curl and OpenSSL"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DEPS_DIR=$(cd $DIR/../deps && pwd)
OPENSSL_SOURCE_DIR=$DEPS_DIR/openssl-1.1.0f/
LIBCURL_SOURCE_DIR=$DEPS_DIR/curl-7.54.1/
AWS_SOURCE_DIR=$DEPS_DIR/aws-sdk-cpp-1.3.50/
PLATFORM=$(echo $(uname) | tr '[:upper:]' '[:lower:]')

target=Release
while getopts ":ht:s:" opt; do
  case $opt in
    t) target=$OPTARG ;;
    h) usage;;
    \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
    :) echo "Option -$OPTARG requires an argument."; >&2 exit 1 ;;
  esac
done

[[ "$target" != "Debug" && "$target" != "Release" ]] && \
    echo "target must be either Debug/Release." && usage

echo "Options:"
echo "  target       = $target"
echo "PATH="$PATH

DEPENDENCY_DIR=$DIR/../deps-build/$PLATFORM
rm -rf $DEPENDENCY_DIR
mkdir -p $DEPENDENCY_DIR

# build openssl
OPENSSL_BUILD_DIR=$DEPENDENCY_DIR/openssl
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

# build libcurl
curl_configure_opts=()
if [[ "$target" != "Release" ]]; then
    curl_configure_opts+=("--enable-debug")
fi
LIBCURL_BUILD_DIR=$DEPENDENCY_DIR/curl
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

# build aws-cpp-sdk
AWS_BUILD_DIR=$DEPENDENCY_LINUX/aws
AWS_CMAKE_BUILD_DIR=$AWS_SOURCE_DIR/cmake-build
aws_configure_opts=()
if [[ "$target" != "Release" ]]; then
    aws_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
else
    aws_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
aws_configure_opts+=(
    "-DCMAKE_C_COMPILER=/usr/local/bin/gcc52"
    "-DCMAKE_CXX_COMPILER=/usr/local/bin/g++52"
    "-DBUILD_ONLY=\"s3\""
    "-DCMAKE_INSTALL_PREFIX=$AWS_BUILD_DIR"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DCMAKE_PREFIX_PATH=\"$LIBCURL_BUILD_DIR/;$OPENSSL_BUILD_DIR/\""
    "-DENABLE_TESTING=OFF"
)

rm -rf $AWS_BUILD_DIR
rm -rf $AWS_CMAKE_BUILD_DIR
mkdir $AWS_BUILD_DIR
mkdir $AWS_CMAKE_BUILD_DIR

cd $AWS_CMAKE_BUILD_DIR
cmake3 ${aws_configure_opts[@]} ../

