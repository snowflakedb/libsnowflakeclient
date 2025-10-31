#!/usr/bin/env bash
set -o pipefail

WIREMOCK_VERSION="3.8.0"
WIREMOCK_JAR="${HOME}/.m2/repository/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"
WIREMOCK_URL="https://repo1.maven.org/maven2/org/wiremock/wiremock-standalone/${WIREMOCK_VERSION}/wiremock-standalone-${WIREMOCK_VERSION}.jar"

mkdir -p "$(dirname "$WIREMOCK_JAR")"
mkdir -p "$(dirname "${HOME}/.wiremock")"


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


JAVA_URL="https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.12%2B7/OpenJDK17U-jdk_x64_linux_hotspot_17.0.12_7.tar.gz"
INSTALL_DIR="${HOME}/.java17"
DOWNLOAD_DIR="${HOME}java17.tar.gz"

mkdir -p $INSTALL_DIR

curl -L --fail $JAVA_URL -o "${DOWNLOAD_DIR}"

tar -xzf /"${DOWNLOAD_DIR}" -C $INSTALL_DIR --strip-components=1

rm "${DOWNLOAD_DIR}"

export JAVA_HOME=$INSTALL_DIR
export PATH=$JAVA_HOME/bin:$PATH

echo "JAVA_HOME: $JAVA_HOME"


function upgrade_java(){
JAVA_MAJOR_VERSION="17"
JAVA_HOME_PATH=""

if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    echo "Cannot detect OS."
    exit 1
fi

echo "---- Finding and Installing OpenJDK $JAVA_MAJOR_VERSION on $OS ----"

if [ "$OS" == "ubuntu" ] || [ "$OS" == "debian" ]; then
    if dpkg -l | grep -q "openjdk-$JAVA_MAJOR_VERSION-jdk"; then
        echo "OpenJDK $JAVA_MAJOR_VERSION was installed"
    else
        echo "OpenJDK $JAVA_MAJOR_VERSION is not installed. Installing now..."
        apt update
        apt install -y openjdk-$JAVA_MAJOR_VERSION-jdk
    fi
    JAVA_HOME_PATH="/usr/lib/jvm/java-$JAVA_MAJOR_VERSION-openjdk-amd64"

elif [ "$OS" == "centos" ] || [ "$OS" == "rhel" ] || [ "$OS" == "fedora" ]; then
    if command -v dnf &> /dev/null; then
        PACKAGE_MANAGER="dnf"
    else
        PACKAGE_MANAGER="yum"
    fi
    
    if $PACKAGE_MANAGER list installed | grep -q "java-$JAVA_MAJOR_VERSION-openjdk"; then
        echo " OpenJDK $JAVA_MAJOR_VERSION was installed."
    else
        echo "OpenJDK $JAVA_MAJOR_VERSION is not installed. Installing now..."
        $PACKAGE_MANAGER install -y java-$JAVA_MAJOR_VERSION-openjdk
    fi
    JAVA_HOME_PATH="/usr/lib/jvm/java-$JAVA_MAJOR_VERSION-openjdk"

else
    echo "Unsupported OS: $OS. Install Java manually."
    exit 1
fi


if [ -z "$JAVA_HOME_PATH" ]; then
    echo "Cannot find the path."
    exit 1
fi

echo "--- 2. JAVA_HOME ($JAVA_HOME_PATH) Update"

export JAVA_HOME="$JAVA_HOME_PATH"
export PATH="$JAVA_HOME/bin:$PATH"

echo "JAVA_HOME is set to $JAVA_HOME "

if command -v alternatives &> /dev/null; then
    echo "--- 3. Setting Java $JAVA_MAJOR_VERSION as the default using alternatives ---"
    if [ "$OS" == "ubuntu" ] || [ "$OS" == "debian" ]; then
        update-alternatives --set java $JAVA_HOME/bin/java
        
    elif [ "$OS" == "centos" ] || [ "$OS" == "rhel" ] || [ "$OS" == "fedora" ]; then
        alternatives --set java $JAVA_HOME/bin/java
    fi
fi

# 최종 버전 확인
echo "--- 최종 Java 버전 확인 ---"
java -version 2>&1
}