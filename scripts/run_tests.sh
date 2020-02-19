#!/bin/bash
#
# Run C API tests
#
function usage() {
    echo "Usage: `basename $0` [-m]"
    echo "-m                 : Use Valgrind. default: no valgrind"
    exit 2
}

#set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh $@

use_valgrind=false
skip_env_args=false
while getopts "hms" opt; do
  case $opt in
    m) use_valgrind=true ;;
    h) usage;;
    s) skip_env_args=true ;;
    \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
    :) echo "Option -$OPTARG requires an argument." >&2; exit 1 ;;
  esac
done

if [[ "$skip_env_args" == "false" ]]; then
    source $DIR/env.sh
fi

ls /mnt/host/

cd /mnt/host
aws s3 cp s3://sfc-jenkins/repository/libsnowflakeclient/linux/test/cmake.tgz cmake.tgz
tar zxvf cmake.tgz

cd /mnt/host/cmake-build
if [[ "$use_valgrind" == "true" ]]; then
    # run valgrind tests
    ctest3 -V -R "valgrind.*"
else
    # run non-valgrind tests
    ctest3 -V -E "valgrind.*"
fi
cd ..
#while true; do sleep 10000; done
