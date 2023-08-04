#!/bin/bash -e
#
# Generate coverage.info file using lcov
#
set -o pipefail

[[ -z "$BUILD_TYPE" ]] && echo "Specify BUILD_TYPE. [Debug, Release]" && exit 1

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $THIS_DIR/..

echo "=== debug gen_lcov.sh THIS_DIR: $THIS_DIR"
working_dir=$PWD/cmake-build-$BUILD_TYPE/CMakeFiles/snowflakeclient.dir/
echo "=== debug gen_lcov.sh working_dir: $working_dir"
if [ -e $working_dir ]; then
    find $working_dir
    echo "=== done: $working_dir"
else
    echo "$working_dir does not exist"
fi

echo "=== debug gen_lcov.sh: installing lcov"
if ! command -v lcov &> /dev/null
then
    echo "lcov could not be found, installing now"
    yum -y install lcov
else
    lcov -v
fi

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
fi

echo "=== debug gen_lcov.sh: running lcov"

lcov -c -d ./cmake-build-$BUILD_TYPE/CMakeFiles/snowflakeclient.dir/ --output-file coverage_unfiltered.info

lcov --remove coverage_unfiltered.info -o coverage.info \
    '/usr/*' \
    '*/lib/cJSON.c' \
    '*/deps-build/*'

genhtml coverage.info --output-directory libsfc_coverage_report

rm coverage_unfiltered.info

FILE=$PWD/coverage.info
if [ -f "$FILE" ]; then
    echo "=== debug: $PWD/coverage.info exists"
else
    echo "=== debug: $PWD/coverage.info does not exist"
fi
