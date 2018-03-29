/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP
#define SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP

#include <string>
#include "crypto/CryptoTypes.hpp"

namespace Snowflake
{
namespace Client
{
struct EncryptionMaterial
{
  /// master key to encrypt file key
  std::string queryStageMasterKey;

  ///  query id
  std::string queryId;

  /// smk id
  long smkId;
};

struct EncryptionMetadata
{
  /// File encryption/decryption key
  Snowflake::Client::Crypto::CryptoIV iv;

  /// File key
  Snowflake::Client::Crypto::CryptoKey fileKey;

  /// base 64 encoded of encrypted file key
  std::string enKekEncoded;

  /// Encryption material descriptor
  std::string matDesc;

  /// encrypted stream size, used for content length
  long long int cipherStreamSize;
};

}
}

#endif //SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP
