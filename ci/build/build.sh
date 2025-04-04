#!/bin/bash -e
#
# Build libsnowflake and its dependencies
#
set -x
set -o pipefail

CI_BUILD_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPTS_DIR=$( cd "$CI_BUILD_DIR/../../scripts" && pwd )

[[ -z "$BUILD_TYPE" ]] && echo "Set BUILD_TYPE: [Debug, Release]" && exit 1

if [[ -z "$UPLOAD_TO_S3" ]]; then
    if [[ -z "$GITHUB_ACTIONS" ]] && [[ -n "$GIT_BRANCH" ]]; then
        UPLOAD_TO_S3=true
    else
        UPLOAD_TO_S3=false
    fi
fi
echo "UPLOAD_TO_S3=${UPLOAD_TO_S3}"

source $SCRIPTS_DIR/_init.sh -t $BUILD_TYPE "$@"
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
        if cp "$CACHE_DIR/$zip_file_name" "$ARTIFACTS_DIR";
        then
            echo "=== using cached: $component_name ==="
            pushd $DEPENDENCY_DIR >& /dev/null
                tar xvfz $ARTIFACTS_DIR/$zip_file_name
            popd >& /dev/null
        else
            echo "=== building dep: $component_name ==="
            "$component_script" -t "$build_type"
            cache_dependency $component_name $component_version $build_type
        fi
    else
        echo "=== download or build $component_name ==="
        ret="$(check_directory $component_name $build_type)"
        if [[ "$ret" == "EXIST" ]] && [[ "$BUILD_CLEAN" == "false" ]]; then
            echo "Skip download or build."
        else
            rm -rf $DEPENDENCY_DIR/$component_name

            src_path="$DEP_URL_PREFIX/$component_name"

            if ! aws s3 cp --only-show-errors $src_path/$zip_file_name $ARTIFACTS_DIR; then
                echo "=== build: $component_name ==="
                "$component_script" -t "$build_type"
                if [[ "$UPLOAD_TO_S3" == "true" && ("$GIT_BRANCH" == "origin/master" || "$GIT_BRANCH" == "master") ]]; then
                    echo "=== upload $component_name"
                    upload_to_sfc_dev1_data $component_name $component_version $build_type
                else
                    echo "No upload $component_name"
                fi
            else
                echo "=== download and extract: $component_name"
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
    local other_args="$4"

    echo "=== build: $component_name ==="
    "$component_script" -t "$build_type" "$other_args"
    local component_version=$("$component_script" -v)

    if [[ "$UPLOAD_TO_S3" == "true" ]]; then
        upload_to_sfc_jenkins $component_name $component_version $build_type
        if [[ "$GIT_BRANCH" == "origin/master" || "$GIT_BRANCH" == "master" ]]; then
            upload_to_sfc_dev1_data $component_name $component_version $build_type
        fi
    fi
}

if [[ "$PLATFORM" == "linux" ]]; then
        download_build_component uuid "$SCRIPTS_DIR/build_uuid.sh" "$target"
fi
download_build_component zlib "$SCRIPTS_DIR/build_zlib.sh" "$target"
download_build_component openssl "$SCRIPTS_DIR/build_openssl.sh" "$target"
download_build_component oob "$SCRIPTS_DIR/build_oob.sh" "$target"
download_build_component curl "$SCRIPTS_DIR/build_curl.sh" "$target"
download_build_component aws "$SCRIPTS_DIR/build_awssdk.sh" "$target"
download_build_component azure "$SCRIPTS_DIR/build_azuresdk.sh" "$target"
download_build_component cmocka "$SCRIPTS_DIR/build_cmocka.sh" "$target"
download_build_component arrow "$SCRIPTS_DIR/build_arrow.sh" "$target"
download_build_component picojson "$SCRIPTS_DIR/build_picojson.sh" "$target"
download_build_component tomlplusplus "$SCRIPTS_DIR/build_tomlplusplus.sh" "$target"

# very tight diskspace limit on github runners, clear deps folder with all .o files
if [[ -n "$GITHUB_ACTIONS" ]]; then
    rm -rf $SCRIPTS_DIR/../deps/*
fi

build_component libsnowflakeclient "$SCRIPTS_DIR/build_libsnowflakeclient.sh" "$target" "$@"


