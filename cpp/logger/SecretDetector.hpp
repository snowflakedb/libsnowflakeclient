#ifndef SNOWFLAKECLIENT_SECRETDETECTOR_HPP
#define SNOWFLAKECLIENT_SECRETDETECTOR_HPP

#include "boost/regex.hpp"

namespace Snowflake
{
namespace Client
{
/**
 * Class SecretDetector
 */
class SecretDetector
{
  public:
    static std::string maskSecrets(std::string); 

  private:
    static boost::regex AWS_KEY_PATTERN;
    static boost::regex AWS_TOKEN_PATTERN;
    static boost::regex SAS_TOKEN_PATTERN;
    static boost::regex PRIVATE_KEY_PATTERN;
    static boost::regex PRIVATE_KEY_DATA_PATTERN;
    static boost::regex CONNECTION_TOKEN_PATTERN;
    static boost::regex PASSWORD_PATTERN;
    static boost::regex ENCRYPTION_CREDS_IN_JSON_PATTERN;
    static boost::regex TOKEN_IN_JSON_PATTERN;
    static boost::regex CURLINFO_TOKEN_PATTERN;

    static std::string maskAwsKeys(std::string text);
    static std::string maskAwsTokens(std::string text);
    static std::string maskSasTokens(std::string text);
    static std::string maskPrivateKey(std::string text);
    static std::string maskPrivateKeyData(std::string text);
    static std::string maskConnectionToken(std::string text);
    static std::string maskPassword(std::string text);
    static std::string maskEncryptioncCredsInJson(std::string text);
    static std::string maskTokenInJson(std::string text);
    static std::string maskCurlInfoToken(std::string text);
};

}
}

#endif /* SNOWFLAKECLIENT_SECRETDETECTOR_HPP */
