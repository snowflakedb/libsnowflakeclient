#!/bin/bash -e
#
# Initialize varizbles

set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DEPS_DIR=$(cd $DIR/../deps && pwd)

PLATFORM=$(echo $(uname) | tr '[:upper:]' '[:lower:]')

# Find cmake, gcc and g++ on target machine. Need cmake 3.0+, gcc/g++ 4.9+
if [[ "$(which cmake3)" ]]; then
    CMAKE="$(which cmake3)"
else
    CMAKE="$(which cmake)"
fi

if [[ -z "$GCC" || -z "$GXX" ]]; then
    if [[ "$(which gcc49)" ]]; then
        GCC="$(which gcc49)"
        GXX="$(which g++49)"
    elif [[ "$(which gcc-4.9)" ]]; then
        GCC="$(which gcc-4.9)"
        GXX="$(which g++-4.9)"
    elif [[ "$(which gcc52)" ]]; then
        GCC="$(which gcc52)"
        GXX="$(which g++52)"
    elif [[ "$(which gcc62)" ]]; then
        GCC="$(which gcc62)"
        GXX="$(which g++62)"
    elif [[ "$(which gcc72)" ]]; then
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
    
    # Check to see if we are doing a universal build
    # By default we do want universal binaries
    if [[ "$UNIVERSAL" == "" ]]; then
        export UNIVERSAL=true
    else
        export UNIVERSAL="${UNIVERSAL}"
    fi

    # Check which arch we are building for if we are not
    # building universal binaries. Note: If UNIVERSAL is 
    # not equal to true, then this parameter has no effect
    if [[ "$ARCH" == "" ]]; then
        export ARCH=x64
    else
        # Ensure that the user specifies the right arch, so 
        # we can skip this check in other scripts
        if [[ "$ARCH" != "x64" && "$ARCH" != "x86" ]]; then
            echo "Invalid arch: $ARCH [Needs to be x86 or x64]"; exit 1;
        fi
        export ARCH="${ARCH}"
    fi
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
