#!/bin/bash -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$DIR/_init.sh" "$@"
source "$DIR/utils.sh"

TOMLPLUSPLUS_SRC_VERSION=3.4.0
TOMLPLUSPLUS_BUILD_VERSION=1
TOMLPLUSPLUS_VERSION=$TOMLPLUSPLUS_SRC_VERSION.$TOMLPLUSPLUS_BUILD_VERSION

SOURCE_DIR="$DIR/../deps/tomlplusplus"
INSTALL_DIR="$DEPENDENCY_DIR/tomlplusplus"

mkdir -p "${INSTALL_DIR}/include"
cp -r "$SOURCE_DIR/include" "${INSTALL_DIR}"

echo === zip_file "tomlplusplus" "$TOMLPLUSPLUS_VERSION" "$target"
zip_file "tomlplusplus" "$TOMLPLUSPLUS_VERSION" "$target"