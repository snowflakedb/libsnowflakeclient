#!/usr/bin/env bash
set -o pipefail

WIREMOCK_VERSION="3.8.0"
WIREMOCK_JAR="${HOME}/.m2/repository/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"
WIREMOCK_URL="https://repo1.maven.org/maven2/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"

mkdir -p "$(dirname "$WIREMOCK_JAR")"

echo "WireMock JAR is downlaoding${WIREMOCK_URL}"
curl -L -o "${WIREMOCK_JAR}" "${WIREMOCK_URL}"
CURL_STATUS=$? 

if [ "$CURL_STATUS" -ne 0 ]; then
    echo "🚨 ERROR: Failed to download WireMock JAR. (curl code: $CURL_STATUS)"
    exit 1
fi

chmod a=rwx "$WIREMOCK_JAR"

echo "SUCCESS: WireMock JAR (${WIREMOCK_VERSION}) downloaded successfully."
echo "   - location: $WIREMOCK_JAR"
echo "   - size: ${FILE_SIZE} bytes"