#!/usr/bin/env bash
set -o pipefail

WIREMOCK_VERSION="3.13.2"
WIREMOCK_JAR="${HOME}/.m2/repository/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"
WIREMOCK_URL="https://repo1.maven.org/maven2/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"
WIREMOCK_SHA256="d097b19bd483c5038479b13a5c71e9faf8f2f5106584f0c120a7770ab0bdb367" # pragma: allowlist secret

mkdir -p "$(dirname "$WIREMOCK_JAR")"

jar_sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

verify_wiremock_jar() {
  [[ -f "$WIREMOCK_JAR" ]] || return 1
  [[ "$(jar_sha256 "$WIREMOCK_JAR")" == "$WIREMOCK_SHA256" ]]
}

download_wiremock_jar() {
  echo "Downloading WireMock JAR from ${WIREMOCK_URL}"
  curl -L --fail -o "${WIREMOCK_JAR}" "${WIREMOCK_URL}"
}

if ! verify_wiremock_jar; then
  echo "WireMock JAR missing or checksum mismatch; redownloading."
  rm -f "$WIREMOCK_JAR"
  download_wiremock_jar
  if ! verify_wiremock_jar; then
    echo "ERROR: WireMock JAR checksum failed after download."
    rm -f "$WIREMOCK_JAR"
    exit 1
  fi
else
  echo "WireMock JAR already present and checksum matches."
fi

chmod +r "$WIREMOCK_JAR"
FILE_SIZE="$(wc -c < "$WIREMOCK_JAR" | tr -d ' ')"

echo "SUCCESS: WireMock JAR (${WIREMOCK_VERSION}) is ready."
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