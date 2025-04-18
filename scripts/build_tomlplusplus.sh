#!/bin/bash -e

TOMLPLUSPLUS_SRC_VERSION=3.4.0
TOMLPLUSPLUS_BUILD_VERSION=2
TOMLPLUSPLUS_VERSION=$TOMLPLUSPLUS_SRC_VERSION.$TOMLPLUSPLUS_BUILD_VERSION

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$DIR/_init.sh" "$@"
source "$DIR/utils.sh"

[[ -n "$GET_VERSION" ]] && echo $TOMLPLUSPLUS_VERSION && exit 0

SOURCE_DIR="$DIR/../deps/tomlplusplus"
INSTALL_DIR="$DEPENDENCY_DIR/tomlplusplus"

mkdir -p "${INSTALL_DIR}/include"
cp -r "$SOURCE_DIR/include" "${INSTALL_DIR}"

echo === zip_file "tomlplusplus" "$TOMLPLUSPLUS_VERSION" "$target"
zip_file "tomlplusplus" "$TOMLPLUSPLUS_VERSION" "$target"