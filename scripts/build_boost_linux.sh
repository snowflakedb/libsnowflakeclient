#!/bin/bash -e
#
# build boost
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build boost"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

BOOST_VERSION=1.75.0
#If its not for XP use gcc52
if [[ -z "$XP_BUILD" ]] ; then
  export CC="/usr/lib64/ccache/gcc52"
  export CXX="/usr/lib64/ccache/g++52"
else
  export CC="gcc82"
  export CXX="g++82"
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@
source $DIR/utils.sh

[[ -n "$GET_VERSION" ]] && echo $BOOST_VERSION && exit 0

BOOST_ZIP=$DEPS_DIR/boost-${BOOST_VERSION}.zip
BOOST_SINGLE_ZIP=$DEPS_DIR/boost.zip
BOOST_SOURCE_DIR=$DEPS_DIR/boost-${BOOST_VERSION}

rm -rf $BOOST_SOURCE_DIR
zip -F $BOOST_ZIP --out $BOOST_SINGLE_ZIP
unzip $BOOST_SINGLE_ZIP -d $DEPS_DIR

BOOST_BUILD_DIR=$DEPENDENCY_DIR/boost
rm -rf $BOOST_BUILD_DIR
mkdir $BOOST_BUILD_DIR

VARIANT="release"
if [[ "$target" != "Release" ]]; then
    VARIANT="debug"
fi

cd $BOOST_SOURCE_DIR
./bootstrap.sh --prefix=. --with-libraries=filesystem,regex,system
./b2 stage --stagedir=$BOOST_BUILD_DIR --includedir=$BOOST_BUILD_DIR/include toolset=gcc variant=$VARIANT link=static address-model=64 cflags="-Wall -m64 -D_REENTRANT -DCLUNIX -fPIC -O3" -a install

cd $DIR

if [[ -z "$GITHUB_ACTIONS" ]]; then
    rm -rf $BOOST_SOURCE_DIR
    rm $BOOST_SINGLE_ZIP
fi
