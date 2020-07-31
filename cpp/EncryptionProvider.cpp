/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <sstream>
#include <iostream>
#include "EncryptionProvider.hpp"
#include "crypto/Cryptor.hpp"
#include "util/Base64.hpp"

void Snowflake::Client::EncryptionProvider::encryptFileKey(
  FileMetadata *fileMetadata, EncryptionMaterial *encryptionMaterial, Crypto::CryptoRandomDevice randomDevice)
{
  char encryptedFileKey[32];
  Crypto::CryptoIV iv;
  Crypto::CryptoKey queryStageMasterKey;
  Crypto::Cryptor::generateIV(iv, randomDevice);

  ::std::string &qsmkEncoded = encryptionMaterial->queryStageMasterKey;

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

void Snowflake::Client::EncryptionProvider::decryptFileKey(
  FileMetadata *fileMetadata, EncryptionMaterial *encryptionMaterial, Crypto::CryptoRandomDevice randomDevice)
{
  const char *encryptedFileKeyEncoded = fileMetadata
    ->encryptionMetadata.enKekEncoded.c_str();
  size_t encryptedFileKeyEncodedSize = fileMetadata
    ->encryptionMetadata.enKekEncoded.size();

  char encryptedFileKey[128];
  Util::Base64::decode(encryptedFileKeyEncoded, encryptedFileKeyEncodedSize,
                       encryptedFileKey);
  size_t encryptedFileKeySize = Util::Base64::decodedLength(
    encryptedFileKeyEncoded,
    encryptedFileKeyEncodedSize);

  Crypto::CryptoIV iv;
  Crypto::CryptoKey queryStageMasterKey;
  Crypto::CryptoKey &fileKey = fileMetadata->encryptionMetadata.fileKey;
  Crypto::Cryptor::generateIV(iv, randomDevice);

  ::std::string &qsmkEncoded = encryptionMaterial->queryStageMasterKey;

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


  context.initialize(Crypto::CryptoOperation::DECRYPT);
  size_t nextSize = context.next(fileKey.data, encryptedFileKey,
                                 encryptedFileKeySize);
  size_t finalizeSize = context.finalize(fileKey.data + nextSize);
  fileKey.nbBits = (nextSize + finalizeSize) *8;
}

void Snowflake::Client::EncryptionProvider::populateFileKeyAndIV(
  FileMetadata *fileMetadata, EncryptionMaterial *encryptionMaterial, Crypto::CryptoRandomDevice randomDevice)
{
  // populate iv
  Crypto::Cryptor::getInstance().generateIV(fileMetadata->encryptionMetadata.iv,
                                            randomDevice);

  // populate file key
  fileMetadata->encryptionMetadata.fileKey.nbBits =
    Util::Base64::decodedLength(
      encryptionMaterial->queryStageMasterKey.data(),
      encryptionMaterial->queryStageMasterKey.size()) * 8;

  Crypto::Cryptor::getInstance().generateKey(
    fileMetadata->encryptionMetadata.fileKey,
    fileMetadata->encryptionMetadata.fileKey.nbBits,
    randomDevice);
}

void Snowflake::Client::EncryptionProvider::serializeEncMatDecriptor(
  FileMetadata *fileMetadata, EncryptionMaterial *encryptionMaterial)
{
  ::std::stringstream ss;
  ss << "{\"queryId\":\"" << encryptionMaterial->queryId << "\", "
     << "\"smkId\":\"" << encryptionMaterial->smkId << "\", "
     << "\"keySize\":\"" << fileMetadata->encryptionMetadata.fileKey.nbBits
     << "\"}";
  fileMetadata->encryptionMetadata.matDesc = ss.str();
}
