#!/bin/bash -e
#
# Build libxml2
# GitHub repo: https://github.com/gnome/libxml2
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Builds libxml2"
    echo "-t <Release/Debug> : Release or Debug builds"
    echo "-v                 : Version"
    exit 2
}
set -o pipefail

XML2_SRC_VERSION=2.15.3
XML2_BUILD_VERSION=1
XML2_VERSION=$XML2_SRC_VERSION.$XML2_BUILD_VERSION

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $XML2_VERSION && exit 0

# build
XML2_BUILD_DIR=$DEPENDENCY_DIR/xml2
rm -rf $XML2_BUILD_DIR
mkdir -p $XML2_BUILD_DIR

XML2_SOURCE_DIR=$DIR/../deps/xml2

rm -rf $XML2_SOURCE_DIR
git clone --single-branch --branch v$XML2_SRC_VERSION --recursive https://github.com/gnome/libxml2.git $XML2_SOURCE_DIR

xml2_configure_opts=()
if [[ "$target" != "Release" ]]; then
    xml2_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
else
    xml2_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
xml2_configure_opts+=(
    "-DCMAKE_C_COMPILER=$GCC"
    "-DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF"
    "-DCMAKE_CXX_COMPILER=$GXX"
    "-DCMAKE_INSTALL_PREFIX=$XML2_BUILD_DIR"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DCMAKE_PREFIX_PATH=\"$LIBCURL_BUILD_DIR/;$OPENSSL_BUILD_DIR/\""
    "-DLIBXML2_WITH_ICONV=OFF"
    "-DLIBXML2_WITH_PYTHON=OFF"
    "-DLIBXML2_WITH_LZMA=OFF"
    "-DZLIB_INCLUDE_DIR=\"$DEPENDENCY_DIR/zlib/include\""
    "-DZLIB_LIBRARY=\"$DEPENDENCY_DIR/zlib/lib/libz.a\""
)

# Check to see if we are doing a universal build or not.
# If we are not doing a universal build, pick an arch to
# build
if [[ "$PLATFORM" == "darwin" ]]; then
    if [[ "$ARCH" == "universal" ]]; then
        echo "[INFO] Building Universal Binary"
        xml2_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64")
    elif [[ "$ARCH" == "x86" ]]; then
        echo "[INFO] Building x86 Binary"
        xml2_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=i386")
    elif [[ "$ARCH" == "x64" ]]; then
        echo "[INFO] Building x64 Binary"
        xml2_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=x86_64")
    else
        echo "[INFO] Building $ARCH Binary"
        xml2_configure_opts+=("-DCMAKE_OSX_ARCHITECTURES=$ARCH")
    fi
    ADDITIONAL_CXXFLAGS="-mmacosx-version-min=${MACOSX_VERSION_MIN}"
fi

cd $XML2_SOURCE_DIR
$CMAKE -E env CXXFLAGS=$ADDITIONAL_CXXFLAGS $CMAKE ${xml2_configure_opts[@]}

make
make install
#azure looking for xml2 headers in libxml
mv $XML2_BUILD_DIR/include/libxml2/libxml $XML2_BUILD_DIR/include/
# keep library in lib folder consistently
if [[ -d "$XML2_BUILD_DIR/lib64" ]]; then
    mv -f $XML2_BUILD_DIR/lib64 $XML2_BUILD_DIR/lib
fi


echo === zip_file "xml2" "$XML2_VERSION" "$target"
zip_file "xml2" "$XML2_VERSION" "$target"
