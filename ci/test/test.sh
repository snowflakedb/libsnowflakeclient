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
set_parameters $CLOUD_PROVIDER
source $SCRIPTS_DIR/env.sh

CLIENT_CODE_COVERAGE=${CLIENT_CODE_COVERAGE:-0}

echo "=== debug test.sh"
CMAKE_DIR=cmake-build-$BUILD_TYPE
if [ -f "/mnt/host/deps-build/linux/Release/libsnowflakeclient/lib/libsnowflakeclient.a" ]; then
    echo "/mnt/host/deps-build/linux/Release/libsnowflakeclient/lib/libsnowflakeclient.a exist"
else
    echo "/mnt/host/deps-build/linux/Release/libsnowflakeclient/lib/libsnowflakeclient.a does not exist"
fi

if [ -f "/mnt/host/$CMAKE_DIR/libsnowflakeclient.a" ]; then
    echo "/mnt/host/$CMAKE_DIR/libsnowflakeclient.a exist"
else
    echo "/mnt/host/$CMAKE_DIR/libsnowflakeclient.a does not exist"
fi
echo "=== debug test.sh ends"

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
    curl -s https://bootstrap.pypa.io/get-pip.py | python >& /dev/null || true
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

function check_gcno()
{
    echo "=== checking if gcno files exist"
    local build_type=$1
    local cmake_dir=cmake-build-$build_type
    if ls $CI_TEST_DIR/../../$cmake_dir/CMakeFiles/snowflakeclient.dir/lib/*.gcno 1> /dev/null 2>&1; then
        echo "=== debug test.sh: $CI_TEST_DIR/../../$cmake_dir/CMakeFiles/snowflakeclient.dir/lib/*.gcno files exist"

        # if [ -e $DEPENDENCY_DIR ]; then
            # echo "$DEPENDENCY_DIR exist"
                # echo "Copy from $CI_TEST_DIR/../../$cmake_dir/libsnowflakeclient.a to $DEPENDENCY_DIR"
                # cp -p $CI_TEST_DIR/../../$cmake_dir/libsnowflakeclient.a $DEPENDENCY_DIR
        # else
            # echo "$DEPENDENCY_DIR does not exist"
        # fi
    else
        echo "=== debug test.sh: $CI_TEST_DIR/../../$cmake_dir/CMakeFiles/snowflakeclient.dir/lib/*.gcno files do not exist"
    fi

    # if ls $CI_TEST_DIR/../../*.gcov 1> /dev/null 2>&1; then
        # echo "=== debug test.sh: $CI_TEST_DIR/../../*.gcov files exist"
    # else
        # echo "=== debug test.sh: $CI_TEST_DIR/../../*.gcov files do not exist"
    # fi
}

function generate_lcov()
{
    echo "=== running lcov"
    # local build_type=$1
    # local cmake_dir=cmake-build-$build_type

    pushd $SCRIPTS_DIR
        bash gen_lcov.sh $BUILD_TYPE
    popd
}

trap drop_schema EXIT

init_python
create_schema
test_component libsnowflakeclient "$SCRIPTS_DIR/build_libsnowflakeclient.sh" "$BUILD_TYPE"
check_gcno "$BUILD_TYPE"
generate_lcov "$BUILD_TYPE"
