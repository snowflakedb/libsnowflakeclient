#!/bin/bash -e
#
# Generate Gcov files
#
set -o pipefail

[[ -z "$BUILD_TYPE" ]] && echo "Specify BUILD_TYPE. [Debug, Release]" && exit 1

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $THIS_DIR/..

for f in lib/*; do
    gcov --preserve-paths --object-directory ./cmake-build-$BUILD_TYPE/CMakeFiles/snowflakeclient.dir/${f}.gcno $f
done

for f in cpp/*; do
    gcov --preserve-paths --object-directory ./cmake-build-$BUILD_TYPE/CMakeFiles/snowflakeclient.dir/${f}.gcno $f
done

for f in cpp/*/*; do
    gcov --preserve-paths --object-directory ./cmake-build-$BUILD_TYPE/CMakeFiles/snowflakeclient.dir/${f}.gcno $f
done

# Remove third-parties source code
if rm *\#deps-build#*.gcov 2> /dev/null ||
   rm *\#lib#cJSON.c.gcov 2> /dev/null ||
   rm *\#usr#*.gcov 2> /dev/null
then
    echo "Removed third-parties source code gcov files"
fi
