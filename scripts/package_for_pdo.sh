#!/bin/bash -e
#
# Remap libsnowflakeclient build output into the pdo_snowflake vendor layout.
# Usage: scripts/package_for_pdo.sh --platform linux|linux-aarch64|darwin [--output pdo-vendor]
#
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
exec python3 "$DIR/package_for_pdo.py" package "$@"
