#!/bin/bash -e
# Utils

UTILS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function init_git_variables()
{
    echo "== set GIT environment variables"
    if [[ -z "$GIT_URL" ]]; then
        export GIT_URL=https://github.com/snowflakedb/libsnowflakeclient.git
    fi
    if [[ -z "$GIT_BRANCH" ]]; then
        BRANCH_NAME=$(git rev-parse --abbrev-ref HEAD)
        export GIT_BRANCH=origin/$BRANCH_NAME
    fi
    if [[ -z "$GIT_COMMIT" ]]; then
        export GIT_COMMIT=$(git rev-parse HEAD)
    fi
    echo "GIT_BRANCH: $GIT_BRANCH, GIT_COMMIT: $GIT_COMMIT"
}

function get_zip_file_name()
{
    local component_name=$1
    local component_version=$2
    local build_type=$3

    [[ -z "$component_name" ]] && echo "Set component name [zlib, openssl, curl, aws, azure, cmocka, libsnowflakeclient]" && exit 1
    [[ -z "$component_version" ]] && echo "Set component_version" && exit 1
    [[ -z "$build_type" ]] && echo "set build_type" && exit 1
    echo "${component_name}_${PLATFORM}_${build_type}-${component_version}.tar.gz"
}

function get_cmake_file_name()
{
    local component_name=$1
    local component_version=$2
    local build_type=$3

    [[ -z "$component_name" ]] && echo "Set component name [zlib, openssl, curl, aws, azure, cmocka, libsnowflakeclient]" && exit 1
    [[ -z "$component_version" ]] && echo "Set component_version" && exit 1
    [[ -z "$build_type" ]] && echo "set build_type" && exit 1
    echo "${component_name}_cmake_${PLATFORM}_${build_type}-${component_version}.tar.gz"
}

function zip_file()
{
    set +x
    set +v
    local component_name=$1
    local component_version=$2
    local build_type=$3

    local zip_file_name=$(get_zip_file_name "$component_name" "$component_version" "$build_type")

    if [[ -z "$GITHUB_ACTIONS" ]]; then
        local f=$UTILS_DIR/../artifacts/$zip_file_name
        rm -f $f
        pushd $DEPENDENCY_DIR/
            echo tar cfz $f $component_name
            tar cfz $f $component_name
            tar tvfz $f
        popd
    fi
}

function check_directory()
{
    local component_name=$1
    local build_type=$2
    if [[ -e "$UTILS_DIR/../deps-build/$PLATFORM/$build_type/$component_name" ]]; then
        echo "EXIST"
    else
        echo "NOT EXIST"
    fi
}

function upload_to_sfc_dev1_data()
{
    local component_name=$1
    local component_version=$2
    local build_type=$3

    local zip_file_name=$(get_zip_file_name $component_name $component_version $build_type)
    aws s3 cp --only-show-errors $UTILS_DIR/../artifacts/$zip_file_name s3://sfc-dev1-data/dependency/$component_name/
}

function upload_to_sfc_jenkins()
{
    local component_name=$1
    local component_version=$2
    local build_type=$3

    echo $GIT_BRANCH

    local zip_file_name=$(get_zip_file_name $component_name $component_version $build_type)
    local target_path=s3://sfc-jenkins/repository/$component_name/$PLATFORM/$git_branch_base_name/$GIT_COMMIT/
    echo "=== uploading artifacts/$zip_file_name to $target_path"
    aws s3 cp --only-show-errors $UTILS_DIR/../artifacts/$zip_file_name $target_path
    local cmake_file_name=$(get_cmake_file_name $component_name $component_version $build_type)
    echo "=== uploading artifacts/$cmake_file_name to $target_path"
    aws s3 cp --only-show-errors $UTILS_DIR/../artifacts/$cmake_file_name $target_path
    local parent_target_path=$(dirname $target_path)
    echo "=== uploading latest_commit to $parent_target_path"
    local latest_commit=$($MKTEMP)
    echo $GIT_COMMIT > $latest_commit
    aws s3 cp --only-show-errors $latest_commit $parent_target_path
    rm -f $latest_commit
}
