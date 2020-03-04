#!/bin/bash -e
#
# Build libsnowflake and its dependencies
#
CI_TEST_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPTS_DIR=$( cd "$CI_TEST_DIR/../../scripts" && pwd )
set -o pipefail

[[ -z "$BUILD_TYPE" ]] && echo "Set BUILD_TYPE: [Debug, Release]" && exit 1

source $SCRIPTS_DIR/_init.sh -t $BUILD_TYPE
echo "CMAKE: $CMAKE, CTEST: $CTEST"
source $SCRIPTS_DIR/utils.sh

init_git_variables
set_parameters $cloud_provider
source $SCRIPTS_DIR/env.sh

echo "=== setting test schema"
if [[ -n "$JOB_NAME" ]]; then
    export SNOWFLAKE_TEST_SCHEMA=JENKINS_${JOB_NAME//-/_}_${BUILD_NUMBER}
elif [[ -n "$GITHUB_ACTIONS" ]]; then
    export SNOWFLAKE_TEST_SCHEMA=${RUNNER_TRACKING_ID//-/_}_${GITHUB_SHA}
fi
echo "SNOWFLAKE_TEST_SCHEMA: $SNOWFLAKE_TEST_SCHEMA"

function test_component()
{
    local component_name=$1
    local component_script=$2
    local build_type=$3

    echo "build_type: $build_type"
    local component_version=$("$component_script" -v)
    local cmake_file_name=$(get_cmake_file_name $component_name $component_version $build_type)
    local cmake_dir=cmake-build-$build_type
    echo "=== test: $component_name"
    if [[ -z "$GITHUB_ACTIONS" ]]; then
        download_from_sfc_jenkins $component_name $component_version $build_type
        pushd $CI_TEST_DIR/../..
            rm -rf $cmake_dir
            tar xvfz artifacts/$cmake_file_name
        popd
    fi
    pushd $CI_TEST_DIR/../..
        cd $cmake_dir
        $CTEST -V -E "valgrind.*"
    popd
}

function init_python()
{
    local temp_dir=$(mktemp -d)
    python3 -m venv $temp_dir/venv
    source $temp_dir/venv/bin/activate
    echo "=== installing pip"
    curl -s https://bootstrap.pypa.io/get-pip.py | python >& /dev/null
    echo "=== installing python connector"
    pip install snowflake-connector-python >&/dev/null
    which python
}

function create_schema()
{
    echo "=== creating schema"
    pushd $SCRIPTS_DIR
        python create_schema.py
    popd
}

function drop_schema()
{
    echo "=== dropping schema"
    pushd $SCRIPTS_DIR
        python drop_schema.py
    popd
}

trap drop_schema EXIT

init_python
create_schema
test_component libsnowflakeclient "$SCRIPTS_DIR/build_libsnowflakeclient.sh" "$BUILD_TYPE"
