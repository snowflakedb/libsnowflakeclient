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

BOOST_SRC_VERSION=1.81.0
BOOST_BUILD_VERSION=1
BOOST_VERSION=${BOOST_SRC_VERSION}.${BOOST_BUILD_VERSION}

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $BOOST_VERSION && exit 0

BOOST_SOURCE_DIR=$DEPS_DIR/boost-${BOOST_SRC_VERSION}
BOOST_BUILD_DIR=$DEPENDENCY_DIR/boost
rm -rf $BOOST_BUILD_DIR
mkdir $BOOST_BUILD_DIR

VARIANT="release"
if [[ "$target" != "Release" ]]; then
    VARIANT="debug"
fi

cd $BOOST_SOURCE_DIR
cat <<EOF > tools/build/src/user-config.jam
using gcc : : $CXX ;
EOF

#When pass CXX to specify the compiler, by default build.sh will set toolset to cxx and skip std=c++11 flag, so we need to set toolset as well
sed -i -- 's/build.sh)/build.sh gcc)/g' bootstrap.sh

# Check to see if we are doing a universal build or not.
# If we are not doing a universal build, build with 64-bit
CXXFLAGS="-std=c++17"
if [[ "$PLATFORM" == "darwin" ]] && [[ "$ARCH" == "universal" ]]; then
    CXX=$CXX ./bootstrap.sh --prefix=. --with-toolset=clang --with-libraries=filesystem,regex,system cxxflags="-arch x86_64 -arch arm64" cflags="-arch x86_64 -arch arm64" linkflags="-arch x86_64 -arch arm64"
    ./b2 stage --stagedir=$BOOST_BUILD_DIR/x64 --includedir=$BOOST_BUILD_DIR/include toolset=clang target-os=darwin architecture=x86 variant=$VARIANT link=static address-model=64 cflags="-Wall -D_REENTRANT -DCLUNIX -fPIC -O3 -arch x86_64" cxxflags="-arch x86_64 ${CXXFLAGS}" linkflags="-arch x86_64" -a install
    ./b2 stage --stagedir=$BOOST_BUILD_DIR/arm64 toolset=clang variant=$VARIANT link=static address-model=64 cflags="-Wall -D_REENTRANT -DCLUNIX -fPIC -O3 -arch arm64" cxxflags="-arch arm64 ${CXXFLAGS}" linkflags="-arch arm64" -a install
    mkdir $BOOST_BUILD_DIR/lib
    for static_lib in $BOOST_BUILD_DIR/x64/lib/*.a; do
        lipo -create -arch x86_64 $static_lib -arch arm64 $BOOST_BUILD_DIR/arm64/lib/$(basename $static_lib) -output $BOOST_BUILD_DIR/lib/$(basename $static_lib);
    done
    rm -rf $BOOST_BUILD_DIR/x64 $BOOST_BUILD_DIR/arm64
else
    CXX=$CXX ./bootstrap.sh --prefix=. --with-toolset=gcc --with-libraries=filesystem,regex,system
    ./b2 stage --stagedir=$BOOST_BUILD_DIR --includedir=$BOOST_BUILD_DIR/include toolset=gcc variant=$VARIANT link=static address-model=64 cxxflags="${CXXFLAGS}" cflags="-Wall -D_REENTRANT -DCLUNIX -fPIC -O3" -a install
fi

cd $DIR

echo === zip_file "boost" "$BOOST_VERSION" "$target"
zip_file "boost" "$BOOST_VERSION" "$target"

if [[ -n "$GITHUB_ACTIONS" ]]; then
    rm -rf $BOOST_SOURCE_DIR
fi
