#!/bin/bash -e
#
# Generate Gcov files
#

for f in $(ls ./lib); do
    gcov --object-directory cmake-build/CMakeFiles/snowflakeclient.dir/lib/${f}.gcno lib/$f
done
