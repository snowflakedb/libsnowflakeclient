#!/bin/bash -e

PICOJSON_SRC_VERSION=1.3.0
PICOJSON_BUILD_VERSION=3
PICOJSON_VERSION=$PICOJSON_SRC_VERSION.$PICOJSON_BUILD_VERSION

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$DIR/_init.sh" "$@"
source "$DIR/utils.sh"

[[ -n "$GET_VERSION" ]] && echo $PICOJSON_VERSION && exit 0

SOURCE_DIR="$DIR/../deps/picojson"
INSTALL_DIR="$DEPENDENCY_DIR/picojson"

mkdir -p "${INSTALL_DIR}/include"
cp "$SOURCE_DIR/picojson.h" "${INSTALL_DIR}/include"

echo === zip_file "picojson" "$PICOJSON_VERSION" "$target"
zip_file "picojson" "$PICOJSON_VERSION" "$target"
