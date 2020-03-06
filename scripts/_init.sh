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

if [[ -z "$GCC" || -z "$GXX" ]]; then
    if which gcc49 >& /dev/null; then
        GCC="$(which gcc49)"
        GXX="$(which g++49)"
    elif which gcc-4.9 >& /dev/null; then
        GCC="$(which gcc-4.9)"
        GXX="$(which g++-4.9)"
    elif which gcc52 >& /dev/null; then
        GCC="$(which gcc52)"
        GXX="$(which g++52)"
    elif which gcc62 >& /dev/null; then
        GCC="$(which gcc62)"
        GXX="$(which g++62)"
    elif which gcc72 >& /dev/null; then
        GCC="$(which gcc72)"
        GXX="$(which g++72)"
    else
        # Default to system
        GCC="$(which gcc)"
        GXX="$(which g++)"
    fi
fi

if [[ "$PLATFORM" == "darwin" ]]; then
    export CC=clang
    export CXX=clang++
    export GCC=$CC
    export GXX=$CXX
    export MACOSX_VERSION_MIN=10.12
    export MKTEMP="mktemp -t snowflake"
    
    # Check to see if we are doing a universal build
    # By default we do want universal binaries
    export ARCH=${ARCH:-universal}

    # Ensure that the user specifies the right arch, so 
    # we can skip this check in other scripts
    if [[ "$ARCH" != "x64" && "$ARCH" != "x86" && "$ARCH" != "universal" ]]; then
        echo "Invalid arch: $ARCH [universal, x86, x64]"; exit 1;
    fi
else
    export MKTEMP="mktemp"
fi

export BUILD_WITH_PROFILE_OPTION=
export BUILD_SOURCE_ONLY=
export GET_VERSION=
target=Release
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
