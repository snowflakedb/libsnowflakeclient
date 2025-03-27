#!/bin/bash -e

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPTS_DIR=$( cd "$THIS_DIR/../../scripts" && pwd )

eval $(jq -r ".authtestparams | to_entries | map(\"export \(.key)=\(.value)\") | .[]" $THIS_DIR/../../.github/workflows/parameters_aws_auth_tests.json)
export SNOWFLAKE_TEST_CA_BUNDLE_FILE=$THIS_DIR/../../cacert.pem

BUILD_TYPE=Release
UPLOAD_TO_S3=true source $THIS_DIR/../build/build.sh

echo "CMAKE: $CMAKE, CTEST: $CTEST"

cmake_dir=cmake-build-$BUILD_TYPE
cd $cmake_dir
$CTEST -V -R "test_auth"
