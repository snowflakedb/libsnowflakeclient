#!/bin/bash -e
#
# set the necessary permissions inside the docker


THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SOURCE_ROOT="$( cd $THIS_DIR/../.. && pwd )"

chmod +x ${SOURCE_ROOT}/*/*.sh
chmod +x ${SOURCE_ROOT}/*/*/*.sh
chmod +x ${SOURCE_ROOT}/deps/*/configure

