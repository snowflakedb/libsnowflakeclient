/*
* Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
*/
#include "SecretDetector.hpp"

namespace Snowflake
{
namespace Client
{
  boost::regex SecretDetector::AWS_KEY_PATTERN = boost::regex("(aws_key_id|aws_secret_key|access_key_id|secret_access_key)(\\s*=\\s*)'([^']+)'", boost::regex::icase);

  boost::regex SecretDetector::AWS_TOKEN_PATTERN = boost::regex("(accessToken|tempToken|keySecret)\"\\s*:\\s*\"([A-Za-z0-9/+]{32,}={0,2})\"", boost::regex::icase);

  boost::regex SecretDetector::SAS_TOKEN_PATTERN = boost::regex("(sig|signature|AWSAccessKeyId|password|passcode)=([A-Za-z0-9%/+]{16,})", boost::regex::icase);

  boost::regex SecretDetector::PRIVATE_KEY_PATTERN = boost::regex("-----BEGIN PRIVATE KEY-----\\\\n([A-Za-z0-9/+=\\\\n]{32,})\\\\n-----END PRIVATE KEY-----", boost::regex::extended | boost::regex::icase);

  boost::regex SecretDetector::PRIVATE_KEY_DATA_PATTERN = boost::regex("\"privateKeyData\": \"([A-Za-z0-9/+=\\\\n]{10,})\"", boost::regex::extended | boost::regex::icase);

  boost::regex SecretDetector::CONNECTION_TOKEN_PATTERN = boost::regex("(token|assertion content)(['\"\\s:=]+)([A-Za-z0-9=/_+-]{8,})", boost::regex::icase);

  boost::regex SecretDetector::PASSWORD_PATTERN = boost::regex("(password|passcode|pwd)(['\"\\s:=]+)([A-Za-z0-9!\"#$%&'\\()*+,-./:;<=>?@\\[\\]^_`\\{|\\}~]{6,})", boost::regex::icase);

  std::string SecretDetector::maskAwsKeys(std::string text)
  {
    return boost::regex_replace(text, SecretDetector::AWS_KEY_PATTERN, "$1$2'****'");
  }

  std::string SecretDetector::maskAwsTokens(std::string text)
  {
    return boost::regex_replace(text, SecretDetector::AWS_TOKEN_PATTERN, "$1\":\"XXXX\"");
  }

  std::string SecretDetector::maskSasTokens(std::string text)
  {
    return boost::regex_replace(text, SecretDetector::SAS_TOKEN_PATTERN, "$1=****");
  }

  std::string SecretDetector::maskPrivateKey(std::string text)
  {
    return boost::regex_replace(text, SecretDetector::PRIVATE_KEY_PATTERN, "-----BEGIN PRIVATE KEY-----\\nXXXX\\n-----END PRIVATE KEY-----");
  }

  std::string SecretDetector::maskPrivateKeyData(std::string text)
  {
    return boost::regex_replace(text, SecretDetector::PRIVATE_KEY_DATA_PATTERN, "\"privateKeyData\": \"XXXX\"");
  }

  std::string SecretDetector::maskConnectionToken(std::string text)
  {
    return boost::regex_replace(text, SecretDetector::CONNECTION_TOKEN_PATTERN, "$1$2****");
  }

  std::string SecretDetector::maskPassword(std::string text)
  {
    return boost::regex_replace(text, SecretDetector::PASSWORD_PATTERN, "$1$2****");
  }

  std::string SecretDetector::maskSecrets(std::string text)
  {
    return SecretDetector::maskAwsKeys(
      SecretDetector::maskAwsTokens(
        SecretDetector::maskSasTokens(
          SecretDetector::maskPrivateKey(
            SecretDetector::maskPrivateKeyData(
              SecretDetector::maskConnectionToken(
                SecretDetector::maskPassword(
                  text
                  )
                )
              )
            )
          )
        )
      );
  }
}
}
