
set -euo pipefail
set -x

VERSION=${1:-}

if [[ -z "$VERSION" ]]; then
    echo "Provide version as an argument"
    exit 1
fi

BRANCH=bump_version-$VERSION
git checkout -b "${BRANCH}"
echo "Bumping version to $VERSION"
awk '{sub(/#define SF_API_VERSION ".*"/,"#define SF_API_VERSION \"'"$VERSION"'\"")}1' include/snowflake/version.h > tmp && mv tmp include/snowflake/version.h
git diff | cat
git add include/snowflake/version.h
git commit -m "Bump version to $VERSION"
git push --set-upstream origin "${BRANCH}"

set +x
echo "--- Create PR using link below ---"
echo "--- https://github.com/snowflakedb/libsnowflakeclient/pull/new/${BRANCH} ---"
