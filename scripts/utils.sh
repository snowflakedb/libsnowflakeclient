#!/bin/bash -e
# Utils

UTILS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

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
