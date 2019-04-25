#!/bin/bash -e
#
# Build libuuid
#

set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@

UTIL_LINUX_TAR_GZ=$DEPS_DIR/util-linux.tar.gz
SOURCE_DIR=$DEPS_DIR/util-linux

tar -xzf $UTIL_LINUX_TAR_GZ -C $DEPS_DIR

if [ ! -d "$SOURCE_DIR" ] ; then
    echo "Could not extract $UTIL_LINUX_TAR_GZ" 
    exit 1
fi

cd $SOURCE_DIR

if [[ "$PLATFORM" == "linux" ]]; then
    echo "Generating UUID build system"
    ./autogen.sh || true
#    Make sure the compiled binary is position independent as ODBC is a shared library
    export CFLAGS="-fPIC"
    export AL_OPTS="-I/usr/share/aclocal"
    ./configure --disable-all-programs --enable-libuuid --prefix=$DEPENDENCY_DIR/uuid  || true
    echo "Compiling UUID source"
    make install  || true
else
    echo "[ERROR] Unknown platform: $PLATFORM"
    exit 1
fi
