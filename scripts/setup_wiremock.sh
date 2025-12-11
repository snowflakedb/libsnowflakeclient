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
    echo "ERROR: Failed to download WireMock JAR. (curl code: $CURL_STATUS)"
    exit 1
fi

chmod +r "$WIREMOCK_JAR"

echo "SUCCESS: WireMock JAR (${WIREMOCK_VERSION}) downloaded successfully."
echo "   - location: $WIREMOCK_JAR"
echo "   - size: ${FILE_SIZE} bytes"

# Detect VM architecture and choose the correct JDK artifact
ARCH="$(uname -m)"
case "$ARCH" in
  aarch64|arm64)
    ARCH_ID="aarch64"
    ;;
  x86_64|amd64)
    ARCH_ID="x64"
    ;;
  *)
    ARCH_ID="x64"
    echo "WARNING: Unrecognized arch '$ARCH', defaulting to x64"
    ;;
esac

JAVA_URL="https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.12%2B7/OpenJDK17U-jdk_${ARCH_ID}_linux_hotspot_17.0.12_7.tar.gz"
INSTALL_DIR="${HOME}/.java17"
DOWNLOAD_DIR="${HOME}/java17.tar.gz"

mkdir -p $INSTALL_DIR

curl -L --fail $JAVA_URL -o "${DOWNLOAD_DIR}"

tar -xzf "${DOWNLOAD_DIR}" -C $INSTALL_DIR --strip-components=1

rm "${DOWNLOAD_DIR}"

export JAVA_HOME=$INSTALL_DIR
export PATH=$JAVA_HOME/bin:$PATH

echo "JAVA_HOME : $JAVA_HOME"
echo "PATH:$PATH"