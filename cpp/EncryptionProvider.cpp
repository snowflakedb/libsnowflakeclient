//
// Created by hyu on 2/5/18.
//

#include <sstream>
#include <iostream>
#include "EncryptionProvider.hpp"
#include "crypto/Cryptor.hpp"
#include "util/Base64.hpp"

void Snowflake::Client::EncryptionProvider::updateEncryptionMetadata(
  FileMetadata *fileMetadata)
{
  populateFileKeyAndIV(fileMetadata);
  encryptFileKey(fileMetadata);
  serializeEncMatDecriptor(fileMetadata);

  // update encrypted stream size
  size_t encryptionBlockSize = Crypto::cryptoAlgoBlockSize(
    Crypto::CryptoAlgo::AES);

  fileMetadata->encryptionMetadata.cipherStreamSize =
    (fileMetadata->srcFileToUploadSize + encryptionBlockSize) /
    encryptionBlockSize * encryptionBlockSize;

}


void Snowflake::Client::EncryptionProvider::encryptFileKey(
  FileMetadata *fileMetadata)
{
  char encryptedFileKey[32];
  Crypto::CryptoIV iv;
  Crypto::CryptoKey queryStageMasterKey;
  Crypto::Cryptor::generateIV(iv, Crypto::CryptoRandomDevice::DEV_RANDOM);

  ::std::string &qsmkEncoded = fileMetadata->encMat->queryStageMasterKey;

  Util::Base64::decode(qsmkEncoded.data(), qsmkEncoded.size(),
                       queryStageMasterKey.data);
  queryStageMasterKey.nbBits = Util::Base64::decodedLength(
    qsmkEncoded.data(), qsmkEncoded.size()) * 8;

  Crypto::CipherContext context =
    Crypto::Cryptor::getInstance().createCipherContext(Crypto::CryptoAlgo::AES,
                                                       Crypto::CryptoMode::ECB,
                                                       Crypto::CryptoPadding::NONE,
                                                       queryStageMasterKey,
                                                       iv);

  Crypto::CryptoKey &fileKey = fileMetadata->encryptionMetadata.fileKey;

  context.initialize(Crypto::CryptoOperation::ENCRYPT);
  size_t nextSize = context.next(encryptedFileKey, fileKey.data,
                                 fileKey.nbBits / 8);
  size_t finalizeSize = context.finalize(encryptedFileKey + nextSize);
  finalizeSize += nextSize;

  // base 64 encode encrypted key
  char encryptedFileKeyEncoded[64];
  Util::Base64::encode(encryptedFileKey, finalizeSize, encryptedFileKeyEncoded);
  size_t encryptedFileKeyEncodedSize = Util::Base64::encodedLength(
    finalizeSize);

  fileMetadata->encryptionMetadata.enKekEncoded =
    ::std::string(encryptedFileKeyEncoded, encryptedFileKeyEncodedSize);
}

void Snowflake::Client::EncryptionProvider::populateFileKeyAndIV(
  FileMetadata *fileMetadata)
{
  // populate iv
  Crypto::Cryptor::getInstance().generateIV(fileMetadata->encryptionMetadata.iv,
                                            Crypto::CryptoRandomDevice::DEV_RANDOM);

  // populate file key
  fileMetadata->encryptionMetadata.fileKey.nbBits =
    Util::Base64::decodedLength(
      fileMetadata->encMat->queryStageMasterKey.data(),
      fileMetadata->encMat->queryStageMasterKey.size()) * 8;

  Crypto::Cryptor::getInstance().generateKey(
    fileMetadata->encryptionMetadata.fileKey,
    fileMetadata->encryptionMetadata.fileKey.nbBits,
    Crypto::CryptoRandomDevice::DEV_RANDOM);
}

void Snowflake::Client::EncryptionProvider::serializeEncMatDecriptor(
  FileMetadata *fileMetadata)
{
  ::std::stringstream ss;
  ss << "{\"queryId\":\"" << fileMetadata->encMat->queryId << "\", "
     << "\"smkId\":\"" << fileMetadata->encMat->smkId << "\", "
     << "\"keySize\":\"" << fileMetadata->encryptionMetadata.fileKey.nbBits
     << "\"}";
  fileMetadata->encryptionMetadata.matDesc = ss.str();
}
