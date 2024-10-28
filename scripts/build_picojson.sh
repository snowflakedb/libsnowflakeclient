#!/bin/bash -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$DIR/_init.sh" "$@"
source "$DIR/utils.sh"

PICOJSON_SRC_VERSION=1.3.0
PICOJSON_BUILD_VERSION=1
PICOJSON_VERSION=$PICOJSON_SRC_VERSION.$PICOJSON_BUILD_VERSION

SOURCE_DIR="$DIR/../deps/picojson"
INSTALL_DIR="$DEPENDENCY_DIR/picojson"

mkdir -p "${INSTALL_DIR}/include"
cp "$SOURCE_DIR/picojson.h" "${INSTALL_DIR}/include"

echo === zip_file "picojson" "$PICOJSON_VERSION" "$target"
zip_file "picojson" "$PICOJSON_VERSION" "$target"
