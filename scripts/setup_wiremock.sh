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

chmod +r "$WIREMOCK_JAR"

echo "SUCCESS: WireMock JAR (${WIREMOCK_VERSION}) downloaded successfully."
echo "   - location: $WIREMOCK_JAR"
echo "   - size: ${FILE_SIZE} bytes"

JAVA_URL="https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.12%2B7/OpenJDK17U-jdk_x64_linux_hotspot_17.0.12_7.tar.gz"
INSTALL_DIR="${HOME}/.java17"
DOWNLOAD_DIR="${HOME}/java17.tar.gz"

mkdir -p $INSTALL_DIR

# curl로 다운로드
curl -L --fail $JAVA_URL -o "${DOWNLOAD_DIR}"

# 압축 해제 및 설치 디렉토리로 이동 (폴더 구조에 따라 --strip-components=1 필요)
tar -xzf "${DOWNLOAD_DIR}" -C $INSTALL_DIR --strip-components=1

# 임시 파일 삭제
rm "${DOWNLOAD_DIR}"