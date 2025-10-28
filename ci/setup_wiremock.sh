#!/usr/bin/env bash
set -o pipefail

WIREMOCK_VERSION="3.8.0"

echo "Downloading WireMock ${WIREMOCK_VERSION} to local Maven repo..."
mvn dependency:get -Dartifact=org.wiremock:wiremock-standalone:${WIREMOCK_VERSION}

WIREMOCK_JAR="${HOME}/.m2/repository/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"

if [ ! -f "$WIREMOCK_JAR" ]; then
  echo "Failed to find $WIREMOCK_JAR"
  exit 1
fi