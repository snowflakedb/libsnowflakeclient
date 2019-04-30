#!/bin/bash -e
#
# Run Travis Build and Tests
#
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export LIB_SNOWFLAKE_CLIENT_DIR=$( cd "$DIR/.." && pwd)
export SNOWFLAKE_TEST_CA_BUNDLE_FILE=$LIB_SNOWFLAKE_CLIENT_DIR/cacert.pem

function travis_fold_start() {
    local name=$1
    local message=$2
    echo "travis_fold:start:$name"
    tput setaf 3 || true
    echo $message
    tput sgr0 || true
    export travis_fold_name=$name
}

function travis_fold_end() {
    echo "travis_fold:end:$travis_fold_name"
    unset travis_fold_name
}

function finish {
    travis_fold_start drop_schema "Drop test schema"
    if [[ "$ENABLE_MOCK_OBJECTS" != "true" ]]; then
        python $DIR/drop_schema.py
    fi
    travis_fold_end
}

travis_fold_start pythonvenv "Set up Python Virtualenv (pyenv)"
pip install -U snowflake-connector-python
travis_fold_end

trap finish EXIT

source $DIR/env.sh

if [[ "$ENABLE_MOCK_OBJECTS" != "true" ]]; then
    travis_fold_start setup_test_data "Setting up test data"
    python $DIR/setup_test_data.py
    travis_fold_end

    travis_fold_start create_schema "Create test schema"
    python $DIR/create_schema.py
    travis_fold_end
fi

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
    # enabling code coverage
    export BUILD_WITH_GCOV_OPTION=true
fi

travis_fold_start build_pdo_snowflake "Build C/C++ library"
$DIR/build_libsnowflakeclient.sh
travis_fold_end

travis_fold_start ctests "Tests C Library"
RUN_TESTS_OPTS=()
if [[ -n "$USE_VALGRIND" ]]; then
    RUN_TESTS_OPTS+=("-m")
fi
$DIR/run_tests.sh "${RUN_TESTS_OPTS[@]}"
travis_fold_end

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
    travis_fold_start ctests "Generate gcov files"
    $DIR/gen_gcov.sh
    travis_fold_end
fi
