#!/usr/bin/env bash
set -o pipefail

WIREMOCK_VERSION="3.8.0"
WIREMOCK_JAR="${HOME}/.m2/repository/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"
WIREMOCK_URL="https://repo1.maven.org/maven2/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"

mkdir -p "$(dirname "$WIREMOCK_JAR")"
curl -L -o "${WIREMOCK_JAR}" "${WIREMOCK_URL}"
chmod +r "$WIREMOCK_JAR"

if [ ! -f "$WIREMOCK_JAR" ]; then
  echo "Failed to find $WIREMOCK_JAR"
  exit 1
fi