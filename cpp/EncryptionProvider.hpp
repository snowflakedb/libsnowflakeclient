/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
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
   * This method will do following:
   * 1) Populate the file key and iv in FileMetadata.EncryptionMetadata.
   * 2) Encrypt file key with query stage master key
   * 3) Serialize encryption material decriptor
   *
   * Also, corresponding FileMetadata.EncryptionMetadata will be updated
   */
  static void updateEncryptionMetadata(FileMetadata *fileMetadata,
                                       EncryptionMaterial *encryptionMaterial);

private:
  /**
   * Generate file key and iv
   */
  static void populateFileKeyAndIV(FileMetadata *fileMetadata,
                                   EncryptionMaterial *encryptionMaterial);

  /**
   * Encrypt file key with query stage master key using AES EBC mode
   */
  static void encryptFileKey(FileMetadata *fileMetadata,
                             EncryptionMaterial *encryptionMaterial);

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
