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
        if BRANCH_NAME=$(git rev-parse --abbrev-ref HEAD); then
            export GIT_BRANCH=origin/$BRANCH_NAME
        else
            echo "[WARN] No GIT_BRANCH is retrieved."
        fi
    fi
    if [[ -z "$GIT_COMMIT" ]]; then
        if ! export GIT_COMMIT=$(git rev-parse HEAD); then
            echo "[WARN} No GIT_COMMIT is retrieved."
        fi
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
    local component_name=$1
    local component_version=$2
    local build_type=$3

    local zip_file_name=$(get_zip_file_name "$component_name" "$component_version" "$build_type")

    if [[ -z "$GITHUB_ACTIONS" ]] && [[ -n "$GIT_BRANCH" ]]; then
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

    local git_branch_base_name=$(echo $GIT_BRANCH | awk -F/ '{print $2}')

    local zip_file_name=$(get_zip_file_name $component_name $component_version $build_type)
    local target_path=s3://sfc-jenkins/repository/$component_name/$PLATFORM/$git_branch_base_name/$GIT_COMMIT/
    echo "=== uploading artifacts/$zip_file_name to $target_path"
    aws s3 cp --only-show-errors $UTILS_DIR/../artifacts/$zip_file_name $target_path
    local cmake_file_name=$(get_cmake_file_name $component_name $component_version $build_type)
    echo "=== uploading artifacts/$cmake_file_name to $target_path"
    aws s3 cp --only-show-errors $UTILS_DIR/../artifacts/$cmake_file_name $target_path
    local parent_target_path=$(dirname $target_path)
    echo "=== uploading latest_commit to $parent_target_path/"
    local latest_commit=$($MKTEMP)
    echo $GIT_COMMIT > $latest_commit
    aws s3 cp --only-show-errors $latest_commit $parent_target_path/latest_commit
    rm -f $latest_commit
}

function download_from_sfc_jenkins()
{
    local component_name=$1
    local component_version=$2
    local build_type=$3

    local git_branch_base_name=$(echo $GIT_BRANCH | awk -F/ '{print $2}')
    
    mkdir -p $UTILS_DIR/../artifacts
    echo "$component_name $component_version $build_type"
    local zip_file_name=$(get_zip_file_name $component_name $component_version $build_type)
    local source_path=s3://sfc-jenkins/repository/$component_name/$PLATFORM/$git_branch_base_name/$GIT_COMMIT
    echo "=== downloading $zip_file_name from $source_path/"
    aws s3 cp --only-show-errors $source_path/$zip_file_name $UTILS_DIR/../artifacts/
    local cmake_file_name=$(get_cmake_file_name $component_name $component_version $build_type)
    echo "=== downloading $cmake_file_name from $source_path/"
    aws s3 cp --only-show-errors $source_path/$cmake_file_name $UTILS_DIR/../artifacts/
}

function set_parameters()
{
    local cloud_provider=$(echo $1 | tr '[:upper:]' '[:lower:]')
    cloud_provider=${cloud_provider:-aws}

    [[ -z "$PARAMETERS_SECRET" ]] && echo "Set PARAMETERS_SECRET" && exit 1

    if [[ "$cloud_provider" == "aws" ]]; then
        echo "== AWS"
        gpg --quiet --batch --yes --decrypt --passphrase="$PARAMETERS_SECRET" --output $UTILS_DIR/../parameters.json $UTILS_DIR/../.github/workflows/parameters_aws_capi.json.gpg
    elif [[ "$cloud_provider" == "azure" ]]; then
        echo "== Azure"
        gpg --quiet --batch --yes --decrypt --passphrase="$PARAMETERS_SECRET" --output $UTILS_DIR/../parameters.json $UTILS_DIR/../.github/workflows/parameters_azure_capi.json.gpg
    elif [[ "$cloud_provider" == "gcp" ]]; then
        echo "== GCP"
        gpg --quiet --batch --yes --decrypt --passphrase="$PARAMETERS_SECRET" --output $UTILS_DIR/../parameters.json $UTILS_DIR/../.github/workflows/parameters_gcp_capi.json.gpg
    else
        echo "Set cloud_provider environment variable: [aws, azure, gcp]"
        exit 1
    fi
}
