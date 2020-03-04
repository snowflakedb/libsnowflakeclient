#!/bin/bash -e
#
# Build libsnowflake and its dependencies
#
set +x
set +v

CI_BUILD_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPTS_DIR=$( cd "$CI_BUILD_DIR/../../scripts" && pwd )
set -o pipefail

[[ -z "$BUILD_TYPE" ]] && echo "Set BUILD_TYPE: [Debug, Release]" && exit 1


source $SCRIPTS_DIR/_init.sh -t $BUILD_TYPE
source $SCRIPTS_DIR/utils.sh
init_git_variables

function download_build_component()
{
    local component_name=$1
    local component_script=$2
    local build_type=$3

    local component_version=$($component_script -v)
    local zip_file_name=$(get_zip_file_name $component_name $component_version $build_type)
    if [[ -n "$GITHUB_ACTIONS" ]]; then
        "$component_script" -t "$build_type"
    else
        echo "=== download or build $component_name ==="
        ret="$(check_directory $component_name $build_type)"
        if [[ "$(check_directory $component_name $build_type)" == "EXIST" ]]; then
            echo "Skip download or build."
        else
            if ! aws s3 cp --only-show-errors s3://sfc-dev1-data/dependency/$component_name/$zip_file_name $ARTIFACTS_DIR; then
                echo "=== build: $component_name ==="
                "$component_script" -t "$build_type"
                if [[ "$GIT_BRANCH" == "origin/master" ]]; then
                    echo "=== upload $component_name"
                    upload_to_sfc_dev1_data $component_name $component_version $build_type
                fi
            else
                echo "=== download and extract: $component_name"
                rm -rf $DEPENDENCY_DIR/$component_name
                pushd $DEPENDENCY_DIR >& /dev/null
                    tar xvfz $ARTIFACTS_DIR/$zip_file_name
                popd >& /dev/null
            fi
        fi
    fi
}

function build_component()
{
    local component_name=$1
    local component_script=$2
    local build_type=$3

    echo "=== build: $component_name ==="
    "$component_script" -t "$build_type"
    local component_version=$("$component_script" -v)
    if [[ -z "$GITHUB_ACTIONS" ]]; then
        echo "=== upload ..."
        upload_to_sfc_jenkins $component_name $component_version $build_type
        if [[ "$GIT_BRANCH" == "origin/master" ]]; then
            upload_to_sfc_dev1_data $component_name $component_version $build_type
        fi
    fi
}

download_build_component zlib "$SCRIPTS_DIR/build_zlib.sh" "$target"
download_build_component openssl "$SCRIPTS_DIR/build_openssl.sh" "$target"
download_build_component curl "$SCRIPTS_DIR/build_curl.sh" "$target"
download_build_component aws "$SCRIPTS_DIR/build_awssdk.sh" "$target"
if [[ "$PLATFORM" == "linux" ]]; then
        download_build_component uuid "$SCRIPTS_DIR/build_uuid.sh" "$target"
fi
download_build_component azure "$SCRIPTS_DIR/build_azuresdk.sh" "$target"
download_build_component cmocka "$SCRIPTS_DIR/build_cmocka.sh" "$target"
build_component libsnowflakeclient "$SCRIPTS_DIR/build_libsnowflakeclient.sh" "$target"
