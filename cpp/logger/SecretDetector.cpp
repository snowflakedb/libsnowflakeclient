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

  // The value class must contain ':' because every session token value starts
  // with a "ver:<n>-" prefix. GlobalServices mints them as
  // "ver:1-hint:<keyId>-<encrypted>", "ver:2-hint:<keyId>-did:<deployId>-<encrypted>"
  // and the V3/V4 equivalents (SecurityToken.java:60-66), so V2 and V4 also carry
  // a ':' in "-did:". Without ':' in the class the value match stops after "ver",
  // which is three characters, below the {8,} minimum, and no session token of any
  // version is masked. Keep ':' where it is: the trailing '-' has to stay the last
  // character of the class so that it remains a literal hyphen, and ':' must not sit
  // next to that '-' (a "+-:" sequence would be read as a range covering ',' '.' '/'
  // and the digits).
  boost::regex SecretDetector::CONNECTION_TOKEN_PATTERN = boost::regex("(token|assertion content|queryStageMasterKey|aws_key_id|aws_secret_key|aws_token)(['\"\\s:=]+)([A-Za-z0-9=/_:+-]{8,})", boost::regex::icase);

  boost::regex SecretDetector::PASSWORD_PATTERN = boost::regex("(password|passcode|pwd)(['\"\\s:=]+)([A-Za-z0-9!\"#$%&'\\()*+,-./:;<=>?@\\[\\]^_`\\{|\\}~]{6,})", boost::regex::icase);

  boost::regex SecretDetector::ENCRYPTION_CREDS_IN_JSON_PATTERN = boost::regex("\"(encryptionMaterial|creds)\"\\s*:\\s*\\{.*?\\}", boost::regex::icase);

  boost::regex SecretDetector::TOKEN_IN_JSON_PATTERN = boost::regex("\"?(mastertoken|token|Snowflake Token|oldSessionToken|sessionToken)\"?(\\s*)?(:|=)(\\.*)?(\\t|\\s+)?\"[a-zA-Z0-9=/_+-:]+\"", boost::regex::icase);

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

  std::string SecretDetector::maskEncryptioncCredsInJson(std::string text)
  {
      return boost::regex_replace(text, SecretDetector::ENCRYPTION_CREDS_IN_JSON_PATTERN, "\"$1\": ****");
  }

  std::string SecretDetector::maskTokenInJson(std::string text)
  {
      return boost::regex_replace(text, SecretDetector::TOKEN_IN_JSON_PATTERN, "\"$1\": ****");
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
                  SecretDetector::maskEncryptioncCredsInJson(
                    SecretDetector::maskTokenInJson(
                      text
                      )
                    )
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
