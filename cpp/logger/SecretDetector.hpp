/*
* Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
*/

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

    static std::string maskAwsKeys(std::string text);
    static std::string maskAwsTokens(std::string text);
    static std::string maskSasTokens(std::string text);
    static std::string maskPrivateKey(std::string text);
    static std::string maskPrivateKeyData(std::string text);
    static std::string maskConnectionToken(std::string text);
    static std::string maskPassword(std::string text);
};

}
}

#endif /* SNOWFLAKECLIENT_SECRETDETECTOR_HPP */
