#!/bin/bash -e
#
# Generate Gcov files
#
set -o pipefail

CMAKE_DIR=$1
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..
for f in $(ls ./lib); do
    gcov --object-directory ./$CMAKE_DIR/CMakeFiles/snowflakeclient.dir/lib/${f}.gcno ./lib/$f
done
