#!/bin/bash -e
#
# Generate coverage.info file using lcov
#
# set -o pipefail

CMAKE_DIR=$1
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

echo "=== debug gen_lcov.sh: $DIR"
working_dir=$PWD/$CMAKE_DIR/CMakeFiles/snowflakeclient.dir/
if [ -e $working_dir ]; then
    find $working_dir
    echo "=== done: $working_dir"
else
    echo "$working_dir does not exist"
fi
echo "=== debug gen_lcov.sh ends"

lcov -c -d ./$CMAKE_DIR/CMakeFiles/snowflakeclient.dir/ --output-file coverage_unfiltered.info

lcov --remove coverage_unfiltered.info -o coverage.info \
    '/usr/*' \
    '*/lib/cJSON.c' \
    '*/deps-build/*'

genhtml coverage.info --output-directory libsfc_coverage_report

rm coverage_unfiltered.info

echo "=== debug gen_lcov.sh: current directory: $PWD"
FILE=$DIR/coverage.info
if [ -f "$FILE" ]; then
    echo "=== debug: $DIR/coverage.info exists"
else
    echo "=== debug: $DIR/coverage.info does not exist"
    find $DIR
    echo "=== done: $DIR"
fi
