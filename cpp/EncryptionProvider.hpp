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
   * Decrypt file key with query stage master key using AES EBC mode.
   *
   * @return
   *    true on success; false if the encryption metadata is invalid (e.g.
   *    a wrapped file key or query stage master key whose base64 form does
   *    not fit the expected buffer.
   */
  static bool decryptFileKey(FileMetadata *fileMetadata,
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
