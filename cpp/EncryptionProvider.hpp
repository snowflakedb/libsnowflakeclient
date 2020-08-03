/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_ENCRYPTIONPROVIDER_HPP
#define SNOWFLAKECLIENT_ENCRYPTIONPROVIDER_HPP

#include <memory>
#include "crypto/CryptoTypes.hpp"
#include "FileTransferAgent.hpp"

namespace Snowflake
{
namespace Client
{
/**
 * Wrapper on top of openssl to do file encryption/decryption
 */
class EncryptionProvider
{
public:
  /**
   * Generate file key and iv
   */
  static void populateFileKeyAndIV(FileMetadata *fileMetadata,
                                   EncryptionMaterial *encryptionMaterial, Crypto::CryptoRandomDevice randomDevice);

  /**
   * Encrypt file key with query stage master key using AES EBC mode
   */
  static void encryptFileKey(FileMetadata *fileMetadata,
                             EncryptionMaterial *encryptionMaterial, Crypto::CryptoRandomDevice randomDevice);
  /**
   * Encrypt file key with query stage master key using AES EBC mode
   */
  static void decryptFileKey(FileMetadata *fileMetadata,
                             EncryptionMaterial *encryptionMaterial, Crypto::CryptoRandomDevice randomDevice);

  /**
   * Serialize Encryption Material descriptor to json string
   * And update encryption metadata
   */
  static void serializeEncMatDecriptor(FileMetadata *fileMetadata,
                                       EncryptionMaterial *encryptionMaterial);

};
}
}


#endif //SNOWFLAKECLIENT_ENCRYPTIONPROVIDER_HPP
