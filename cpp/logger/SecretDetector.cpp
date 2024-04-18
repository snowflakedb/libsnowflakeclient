/*
* Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
*/
#include "SecretDetector.hpp"

using namespace boost::xpressive;
using namespace regex_constants;

namespace Snowflake
{
namespace Client
{
  sregex SecretDetector::AWS_KEY_PATTERN = sregex::compile("(aws_key_id|aws_secret_key|access_key_id|secret_access_key)(\\s*=\\s*)'([^']+)'", icase);

  sregex SecretDetector::AWS_TOKEN_PATTERN = sregex::compile("(accessToken|tempToken|keySecret)\"\\s*:\\s*\"([A-Za-z0-9/+]{32,}={0,2})\"", icase);

  sregex SecretDetector::SAS_TOKEN_PATTERN = sregex::compile("(sig|signature|AWSAccessKeyId|password|passcode)=([A-Za-z0-9%/+]{16,})", icase);

  sregex SecretDetector::PRIVATE_KEY_PATTERN = sregex::compile("-----BEGIN PRIVATE KEY-----\\\\n([A-Za-z0-9/+=\\\\n]{32,})\\\\n-----END PRIVATE KEY-----", icase);

  sregex SecretDetector::PRIVATE_KEY_DATA_PATTERN = sregex::compile("\"privateKeyData\": \"([A-Za-z0-9/+=\\\\n]{10,})\"", icase);

  sregex SecretDetector::CONNECTION_TOKEN_PATTERN = sregex::compile("(token|assertion content)(['\"\\s:=]+)([A-Za-z0-9=/_+-]{8,})", icase);

  sregex SecretDetector::PASSWORD_PATTERN = sregex::compile("(password|passcode|pwd)(['\"\\s:=]+)([A-Za-z0-9!\"#$%&'\\()*+,-./:;<=>?@\\[\\]^_`\\{|\\}~]{6,})", icase);

  std::string SecretDetector::maskAwsKeys(std::string text)
  {
    return regex_replace(text, AWS_KEY_PATTERN, "$1$2'****'");
  }

  std::string SecretDetector::maskAwsTokens(std::string text)
  {
    return regex_replace(text, AWS_TOKEN_PATTERN, "$1\":\"XXXX\"");
  }

  std::string SecretDetector::maskSasTokens(std::string text)
  {
    return regex_replace(text, SAS_TOKEN_PATTERN, "$1=****");
  }

  std::string SecretDetector::maskPrivateKey(std::string text)
  {
    return regex_replace(text, PRIVATE_KEY_PATTERN, "-----BEGIN PRIVATE KEY-----\\nXXXX\\n-----END PRIVATE KEY-----");
  }

  std::string SecretDetector::maskPrivateKeyData(std::string text)
  {
    return regex_replace(text, PRIVATE_KEY_DATA_PATTERN, "\"privateKeyData\": \"XXXX\"");
  }

  std::string SecretDetector::maskConnectionToken(std::string text)
  {
    return regex_replace(text, CONNECTION_TOKEN_PATTERN, "$1$2****");
  }

  std::string SecretDetector::maskPassword(std::string text)
  {
    return regex_replace(text, PASSWORD_PATTERN, "$1$2****");
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
