#!/bin/bash -e
#
# Generate Gcov files
#
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..
for f in $(ls ./lib); do
    gcov --object-directory ./cmake-build/CMakeFiles/snowflakeclient.dir/lib/${f}.gcno ./lib/$f
done
