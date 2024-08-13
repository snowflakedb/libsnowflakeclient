#!/bin/bash -e
#
# Initialize varizbles

set -o pipefail

export PATH=/usr/local/bin:$PATH
export TERM=vt100

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DEPS_DIR=$(cd $DIR/../deps && pwd)
ARTIFACTS_DIR=$DIR/../artifacts
mkdir -p $ARTIFACTS_DIR

PLATFORM=$(echo $(uname) | tr '[:upper:]' '[:lower:]')

# Find cmake, gcc and g++ on target machine. Need cmake 3.0+, gcc/g++ 4.9+
if which cmake3 >& /dev/null; then
    export CMAKE="$(which cmake3)"
    export CTEST="$(which ctest3)"
else
    export CMAKE="$(which cmake)"
    export CTEST="$(which ctest)"
fi

# for XP_BUILD try gcc82 first
if [[ -n "$XP_BUILD" ]] ; then
    if which gcc-82 >& /dev/null; then
        GCC="$(which gcc-82)"
        GXX="$(which g++-82)"
    elif which gcc82 >& /dev/null; then
        GCC="$(which gcc82)"
        GXX="$(which g++82)"
    elif which gcc-8 >& /dev/null; then
        GCC="$(which gcc-8)"
        GXX="$(which g++-8)"
    elif which gcc8 >& /dev/null; then
        GCC="$(which gcc8)"
        GXX="$(which g++8)"
    fi
fi

# Otherwise try gcc52 first and default to gcc.
if [[ -z "$GCC" || -z "$GXX" ]]; then
    if which gcc-52 >& /dev/null; then
        GCC="$(which gcc-52)"
        GXX="$(which g++-52)"
    elif which gcc52 >& /dev/null; then
        GCC="$(which gcc52)"
        GXX="$(which g++52)"
# Don't know why azure build script use gcc in /usr/lib64/ccache but let's keep it
    elif (test -f "/usr/lib64/ccache/gcc52") && (test -f "/usr/lib64/ccache/g++52"); then
        GCC="/usr/lib64/ccache/gcc52"
        GXX="/usr/lib64/ccache/g++52"
    elif which gcc-5 >& /dev/null; then
        GCC="$(which gcc-5)"
        GXX="$(which g++-5)"
    elif which gcc5 >& /dev/null; then
        GCC="$(which gcc5)"
        GXX="$(which g++5)"
    elif which gcc-62 >& /dev/null; then
        GCC="$(which gcc-62)"
        GXX="$(which g++-62)"
    elif which gcc62 >& /dev/null; then
        GCC="$(which gcc62)"
        GXX="$(which g++62)"
    elif which gcc-6 >& /dev/null; then
        GCC="$(which gcc-6)"
        GXX="$(which g++-6)"
    elif which gcc6 >& /dev/null; then
        GCC="$(which gcc6)"
        GXX="$(which g++6)"
    elif which gcc-72 >& /dev/null; then
        GCC="$(which gcc-72)"
        GXX="$(which g++-72)"
    elif which gcc72 >& /dev/null; then
        GCC="$(which gcc72)"
        GXX="$(which g++72)"
    elif which gcc-7 >& /dev/null; then
        GCC="$(which gcc-7)"
        GXX="$(which g++-7)"
    elif which gcc7 >& /dev/null; then
        GCC="$(which gcc7)"
        GXX="$(which g++7)"
    elif which gcc-82 >& /dev/null; then
        GCC="$(which gcc-82)"
        GXX="$(which g++-82)"
    elif which gcc82 >& /dev/null; then
        GCC="$(which gcc82)"
        GXX="$(which g++82)"
    elif which gcc-8 >& /dev/null; then
        GCC="$(which gcc-8)"
        GXX="$(which g++-8)"
    elif which gcc8 >& /dev/null; then
        GCC="$(which gcc8)"
        GXX="$(which g++8)"
    elif which gcc-92 >& /dev/null; then
        GCC="$(which gcc-92)"
        GXX="$(which g++-92)"
    elif which gcc92 >& /dev/null; then
        GCC="$(which gcc92)"
        GXX="$(which g++92)"
    elif which gcc-9 >& /dev/null; then
        GCC="$(which gcc-9)"
        GXX="$(which g++-9)"
    elif which gcc9 >& /dev/null; then
        GCC="$(which gcc9)"
        GXX="$(which g++9)"
    else
        # Default to system
        GCC="$(which gcc)"
        GXX="$(which g++)"
    fi
fi

if [[ -z "$CC" || -z "$CXX" ]]; then
    CC=$GCC
    CXX=$GXX
fi

export GCCVERSION="$($GCC --version | grep ^gcc | sed 's/^.* //g')"

# Moving linux build to centos7 so ARROW_FROM_SOURCE should always
# be turned on.
# Keep ARROW_FROM_SOURCE for now in case we need to
# disable it for some reason. Eventually will remove it when the
# build on centos7 is stable.
if [[ -z "$ARROW_FROM_SOURCE" ]]; then
    export ARROW_FROM_SOURCE=1
fi

if [[ "$PLATFORM" == "darwin" ]]; then
    export CC=clang
    export CXX=clang++
    export GCC=$CC
    export GXX=$CXX
    export MACOSX_VERSION_MIN=10.14
    export MKTEMP="mktemp -t snowflake"
    
    # Check to see if we are doing a universal build
    # By default we do want universal binaries
    if [[ "aarch64" == $(uname -p) ]]; then
        export ARCH=${ARCH:-aarch64}
    else
        export ARCH=${ARCH:-universal}
    fi

    # Ensure that the user specifies the right arch, so 
    # we can skip this check in other scripts
    if [[ "$ARCH" != "x64" && "$ARCH" != "x86" && "$ARCH" != "universal" && "$ARCH" != "aarch64" && "$ARCH" != "arm64" ]]; then
        echo "Invalid arch: $ARCH [universal, x86, x64]"; exit 1;
    fi
else
    export MKTEMP="mktemp"
fi

export BUILD_WITH_PROFILE_OPTION=
export BUILD_SOURCE_ONLY=
export GET_VERSION=
if [ -z "$target" ]; then
    target="Release"
fi
while getopts ":hvpt:s" opt; do
  case $opt in
    t) target=$OPTARG ;;
    p) export BUILD_WITH_PROFILE_OPTION=true ;;
    h) usage;;
    s) export BUILD_SOURCE_ONLY=true ;;
    v) export GET_VERSION=true ;;
    \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
    :) echo "Option -$OPTARG requires an argument."; >&2 exit 1 ;;
  esac
done

[[ "$target" != "Debug" && "$target" != "Release" ]] && \
    echo "target must be either Debug/Release." && usage

if [[ -z "$GET_VERSION" ]]; then
    echo "Options:"
    echo "  target       = $target"
    echo "PATH="$PATH
fi

export DEPENDENCY_DIR=$DIR/../deps-build/$PLATFORM/$target
mkdir -p $DEPENDENCY_DIR
