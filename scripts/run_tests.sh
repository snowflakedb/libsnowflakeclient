#!/bin/bash -e
#
# Run C API tests
#
function usage() {
    echo "Usage: `basename $0` [-m]"
    echo "-m                 : Use Valgrind. default: no valgrind"
    exit 2
}

set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/env.sh

use_valgrind=false
while getopts "hm" opt; do
  case $opt in
    m) use_valgrind=true ;;
    h) usage;;
    \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
    :) echo "Option -$OPTARG requires an argument." >&2; exit 1 ;;
  esac
done


cd "./cmake-build"
if [[ "$use_valgrind" == "true" ]]; then
    # run valgrind tests
    ctest -V -R "valgrind.*"
else
    # run non-valgrind tests
    ctest -V -E "valgrind.*"
fi
cd ..
