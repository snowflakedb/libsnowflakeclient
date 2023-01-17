#!/bin/bash -e
#
# build boost
# GitHub repo: https://github.com/boostorg/boost.git
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build boost"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

BOOST_VERSION=1.75.0

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $BOOST_VERSION && exit 0

BOOST_SOURCE_DIR=$DEPS_DIR/boost-${BOOST_VERSION}
BOOST_BUILD_DIR=$DEPENDENCY_DIR/boost
rm -rf $BOOST_BUILD_DIR
mkdir $BOOST_BUILD_DIR

VARIANT="release"
if [[ "$target" != "Release" ]]; then
    VARIANT="debug"
fi

cd $BOOST_SOURCE_DIR
echo "using gcc : : $CXX ; " >> tools/build/src/user-config.jam
#When pass CXX to specify the compiler, by default build.sh will set toolset to cxx and skip std=c++11 flag, so we need to set toolset as well
sed -i -- 's/build.sh)/build.sh gcc)/g' bootstrap.sh
CXX=$CXX ./bootstrap.sh --prefix=. --with-toolset=gcc --with-libraries=filesystem,regex,system

# Check to see if we are doing a universal build or not.
# If we are not doing a universal build, build with 64-bit
if [[ "$PLATFORM" == "darwin" ]] && [[ "$ARCH" == "universal" ]]; then
    ./b2 stage --stagedir=$BOOST_BUILD_DIR/64 --includedir=$BOOST_BUILD_DIR/include toolset=gcc variant=$VARIANT link=static address-model=64 cflags="-Wall -D_REENTRANT -DCLUNIX -fPIC -O3" -a install
    ./b2 stage --stagedir=$BOOST_BUILD_DIR/32 toolset=gcc variant=$VARIANT link=static address-model=32 cflags="-Wall -D_REENTRANT -DCLUNIX -fPIC -O3" -a install
    mkdir $BOOST_BUILD_DIR/lib
    for static_lib in $BOOST_BUILD_DIR/64/lib/*.a; do
        lipo -create -arch x86_64 $static_lib -arch i386 $BOOST_BUILD_DIR/32/lib/$(basename $static_lib) -output $BOOST_BUILD_DIR/lib/$(basename $static_lib);
    done
    rm -rf $BOOST_BUILD_DIR/64 $BOOST_BUILD_DIR/32
else
    ./b2 stage --stagedir=$BOOST_BUILD_DIR --includedir=$BOOST_BUILD_DIR/include toolset=gcc variant=$VARIANT link=static address-model=64 cflags="-Wall -D_REENTRANT -DCLUNIX -fPIC -O3" -a install
fi

cd $DIR

echo === zip_file "boost" "$BOOST_VERSION" "$target"
zip_file "boost" "$BOOST_VERSION" "$target"

if [[ -n "$GITHUB_ACTIONS" ]]; then
    rm -rf $BOOST_SOURCE_DIR
    rm $BOOST_SINGLE_ZIP
fi
