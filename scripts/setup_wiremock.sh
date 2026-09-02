#!/usr/bin/env bash
set -o pipefail

WIREMOCK_VERSION="3.13.2"
WIREMOCK_JAR="${HOME}/.m2/repository/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"
WIREMOCK_SHA256="d097b19bd483c5038479b13a5c71e9faf8f2f5106584f0c120a7770ab0bdb367" # pragma: allowlist secret
WIREMOCK_ARTIFACT_PATH="org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"
WIREMOCK_ARTIFACTORY_BASE_URL="https://artifactory.ci1.us-west-2.aws-dev.app.snowflake.com/artifactory/development-maven-virtual"
WIREMOCK_MAVEN_CENTRAL_BASE_URL="https://repo1.maven.org/maven2"
DOWNLOAD_ATTEMPTS="${WIREMOCK_DOWNLOAD_ATTEMPTS:-5}"
DOWNLOAD_RETRY_DELAY="${WIREMOCK_DOWNLOAD_RETRY_DELAY:-2}"

mkdir -p "$(dirname "$WIREMOCK_JAR")"

# Retries are implemented here rather than with curl's --retry-all-errors,
# which is unavailable in the curl 7.61 shipped by the RHEL8/Rocky8 images.
download_with_retries() {
  local url="$1"
  local output="$2"
  local attempt=1
  local delay="${DOWNLOAD_RETRY_DELAY}"

  while true; do
    if curl -L --fail -o "${output}" "${url}"; then
      return 0
    fi

    if [ "${attempt}" -ge "${DOWNLOAD_ATTEMPTS}" ]; then
      return 1
    fi

    echo "WARNING: Download attempt ${attempt}/${DOWNLOAD_ATTEMPTS} failed for ${url}; retrying in ${delay}s"
    sleep "${delay}"
    attempt=$((attempt + 1))
    delay=$((delay * 2))
  done
}

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

wiremock_download_base_urls() {
  if [[ -n "${WIREMOCK_BASE_URL:-}" ]]; then
    echo "${WIREMOCK_BASE_URL}"
    return
  fi

  if [[ -n "${JENKINS_HOME:-}" ]] || [[ -n "${JOB_NAME:-}" ]]; then
    echo "${WIREMOCK_ARTIFACTORY_BASE_URL}"
    echo "${WIREMOCK_MAVEN_CENTRAL_BASE_URL}"
    return
  fi

  echo "${WIREMOCK_MAVEN_CENTRAL_BASE_URL}"
}

download_wiremock_jar() {
  local temp_jar="${WIREMOCK_JAR}.tmp"
  local base_url url

  rm -f "${temp_jar}"

  for base_url in $(wiremock_download_base_urls); do
    url="${base_url}/${WIREMOCK_ARTIFACT_PATH}"
    echo "Downloading WireMock JAR from ${url}"
    if download_with_retries "${url}" "${temp_jar}"; then
      mv "${temp_jar}" "${WIREMOCK_JAR}"
      return 0
    fi

    echo "WARNING: Failed to download WireMock JAR from ${url}"
    rm -f "${temp_jar}"
  done

  return 1
}

if ! verify_wiremock_jar; then
  echo "WireMock JAR missing or checksum mismatch; redownloading."
  rm -f "$WIREMOCK_JAR"
  if ! download_wiremock_jar; then
    echo "ERROR: Failed to download WireMock JAR from all configured sources."
    rm -f "$WIREMOCK_JAR"
    exit 1
  fi
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

if ! download_with_retries "$JAVA_URL" "${DOWNLOAD_DIR}"; then
  echo "ERROR: Failed to download JDK from ${JAVA_URL}"
  exit 1
fi

tar -xzf "${DOWNLOAD_DIR}" -C $INSTALL_DIR --strip-components=1

rm "${DOWNLOAD_DIR}"

export JAVA_HOME=$INSTALL_DIR
export PATH=$JAVA_HOME/bin:$PATH

echo "JAVA_HOME : $JAVA_HOME"
echo "PATH:$PATH"
