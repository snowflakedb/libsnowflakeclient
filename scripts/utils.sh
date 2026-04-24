#!/bin/bash -e
# Utils

UTILS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

PLATFORM_ARCH=$(uname -m)
if [[ "$PLATFORM_ARCH" == "x86_64" ]] || [[ "$PLATFORM_ARCH" == "i386" ]]; then
  export REP_URL_PREFIX="s3://sfc-eng-jenkins/repository"
  export DEP_URL_PREFIX="s3://sfc-eng-data/dependency"
else
  export REP_URL_PREFIX="s3://sfc-eng-jenkins/repository-$PLATFORM_ARCH"
  export DEP_URL_PREFIX="s3://sfc-eng-data/dependency-$PLATFORM_ARCH"
fi

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

    if [[ -n "$GIT_BRANCH" ]]; then
        local f=$UTILS_DIR/../artifacts/$zip_file_name
        rm -f $f
        pushd $DEPENDENCY_DIR/
            echo tar cfz $f $component_name
            tar cfz $f $component_name
            tar tvfz $f
        popd
    fi
}

function zip_files()
{
    local component_name=$1
    local component_version=$2
    local build_type=$3
    local files=$4

    local zip_file_name=$(get_zip_file_name "$component_name" "$component_version" "$build_type")

    if [[ -n "$GIT_BRANCH" ]]; then
        local f=$UTILS_DIR/../artifacts/$zip_file_name
        rm -f $f
        pushd $DEPENDENCY_DIR/
            echo tar cfz $f $files
            tar cfz $f $files
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
    aws s3 cp --only-show-errors $UTILS_DIR/../artifacts/$zip_file_name $DEP_URL_PREFIX/$component_name/
}

function cache_dependency()
{

    local component_name=$1
    local component_version=$2
    local build_type=$3

    local zip_file_name=$(get_zip_file_name $component_name $component_version $build_type)
    mkdir -p $CACHE_DIR
    cp $UTILS_DIR/../artifacts/$zip_file_name "$CACHE_DIR/"
}

function upload_to_sfc_jenkins()
{
    local component_name=$1
    local component_version=$2
    local build_type=$3

    local git_branch_base_name=$(echo $GIT_BRANCH | awk -F/ '{print $2}')

    local zip_file_name=$(get_zip_file_name $component_name $component_version $build_type)
    local target_path=$REP_URL_PREFIX/$component_name/$PLATFORM/$git_branch_base_name/$GIT_COMMIT/
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
    local source_path=$REP_URL_PREFIX/$component_name/$PLATFORM/$git_branch_base_name/$GIT_COMMIT
    echo "=== downloading $zip_file_name from $source_path/"
    aws s3 cp --only-show-errors $source_path/$zip_file_name $UTILS_DIR/../artifacts/
    local cmake_file_name=$(get_cmake_file_name $component_name $component_version $build_type)
    echo "=== downloading $cmake_file_name from $source_path/"
    aws s3 cp --only-show-errors $source_path/$cmake_file_name $UTILS_DIR/../artifacts/
}

function set_parameters()
{
    local cloud_provider=$(echo $1 | tr '[:lower:]' '[:upper:]')
    CLOUD_PROVIDER=${cloud_provider:-AWS}

    [[ -z "$PARAMETERS_SECRET" ]] && echo "Set PARAMETERS_SECRET" && exit 1

    local repo_root="$UTILS_DIR/.."
    local params_dir="$repo_root/.github/workflows"
    local source_rsa_dir="$params_dir/rsa_keys"
    local target_rsa_dir="$repo_root/rsa_keys"
    local source_params=""
    local rsa_key_basename=""

    if [[ "$CLOUD_PROVIDER" == "AWS" ]]; then
        echo "== AWS"
        source_params="$params_dir/parameters_aws_capi.json.gpg"
        rsa_key_basename="rsa_key_libsfclient_aws"
    elif [[ "$CLOUD_PROVIDER" == "AZURE" ]]; then
        echo "== AZURE"
        source_params="$params_dir/parameters_azure_capi.json.gpg"
        rsa_key_basename="rsa_key_libsfclient_azure"
    elif [[ "$CLOUD_PROVIDER" == "GCP" ]]; then
        echo "== GCP"
        source_params="$params_dir/parameters_gcp_capi.json.gpg"
        rsa_key_basename="rsa_key_libsfclient_gcp"
    else
        echo "Set CLOUD_PROVIDER environment variable: [AWS, AZURE, GCP]"
        exit 1
    fi

    gpg --quiet --batch --yes --decrypt --passphrase="$PARAMETERS_SECRET" --output "$repo_root/parameters.json" "$source_params"

    # The encrypted key is named *.json.gpg (historical convention) but contains
    # a PEM private key; we decrypt it to a *.p8 file under <repo>/rsa_keys so
    # that the relative path in parameters.json (rsa_keys/<basename>.p8)
    # resolves correctly regardless of the working directory.
    decrypt_rsa_key \
        "$source_rsa_dir/${rsa_key_basename}.json.gpg" \
        "$target_rsa_dir/${rsa_key_basename}.p8"
}

# Decrypts the cloud-specific RSA private key used for keypair (JWT)
# authentication. The key passphrase is taken from PRIVATEKEY_CSP_KEY, with
# PARAMETERS_SECRET as a fallback for environments that share a single secret.
# A missing source file is not fatal so the password-based flow keeps working
# until the encrypted keys are added.
function decrypt_rsa_key()
{
    local source_rsa_key=$1
    local target_rsa_key=$2

    if [[ ! -f "$source_rsa_key" ]]; then
        echo "[WARN] No RSA key file found at $source_rsa_key, skipping keypair setup."
        return 0
    fi

    local rsa_secret=${PRIVATEKEY_CSP_KEY:-$PARAMETERS_SECRET}
    if [[ -z "$rsa_secret" ]]; then
        echo "[WARN] Neither PRIVATEKEY_CSP_KEY nor PARAMETERS_SECRET is set, skipping keypair setup."
        return 0
    fi

    local secret_source="PRIVATEKEY_CSP_KEY"
    if [[ -z "$PRIVATEKEY_CSP_KEY" ]]; then
        secret_source="PARAMETERS_SECRET (fallback; PRIVATEKEY_CSP_KEY not set)"
    fi

    mkdir -p "$(dirname "$target_rsa_key")"
    if ! gpg --quiet --batch --yes --decrypt --passphrase="$rsa_secret" --output "$target_rsa_key" "$source_rsa_key" 2>/dev/null; then
        echo "[ERROR] gpg failed to decrypt $source_rsa_key using $secret_source."
        echo "[ERROR] Verify that the GitHub Actions secret matches the passphrase used to encrypt the file."
        rm -f "$target_rsa_key"
        return 1
    fi
    chmod 600 "$target_rsa_key" 2>/dev/null || true
    echo "[INFO] Decrypted RSA key to $target_rsa_key (passphrase from $secret_source)"
}

