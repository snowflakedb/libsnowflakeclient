/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SIGNER_HPP
#define SNOWFLAKECLIENT_SIGNER_HPP


#include <memory>
#include <string>

namespace Snowflake
{
namespace Jwt
{

enum class AlgorithmType
{
  //TODO list other algorithm types here
  RS256,
};

class Signer
{
public:
  // TODO get rid of default signer constructor
  static Signer *BuildSigner(AlgorithmType type);

  std::string sign(std::string);

  std::string decode(std::string);
};

class RSA256Signer : Signer
{
  AlgorithmType type;


};
} // namespace Jwt
} // namespace Snowflake


#endif //SNOWFLAKECLIENT_SIGNER_HPP
