#!/bin/bash -e
#
# Generate Gcov files
#
set -o pipefail

[[ -z "$BUILD_TYPE" ]] && echo "Specify BUILD_TYPE. [Debug, Release]" && exit 1

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $THIS_DIR/..

if ! command -v lcov &> /dev/null
then
    echo "lcov could not be found"
else
    lcov -v
fi

if ! command -v gcov &> /dev/null
then
    echo "gcov could not be found"
else
    gcov -v
    gcc -v
fi

echo "=== debug gen_gcov.sh: running gcov"

for f in lib/*; do
    gcov --preserve-paths --object-directory ./cmake-build-$BUILD_TYPE/CMakeFiles/snowflakeclient.dir/${f}.gcno $f
done

for f in cpp/*/*; do
    gcov --preserve-paths --object-directory ./cmake-build-$BUILD_TYPE/CMakeFiles/snowflakeclient.dir/${f}.gcno $f
done
