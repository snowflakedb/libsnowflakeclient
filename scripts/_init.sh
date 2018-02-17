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

target=Release
while getopts ":ht:" opt; do
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
mkdir -p $DEPENDENCY_DIR
