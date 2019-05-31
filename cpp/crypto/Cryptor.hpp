/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_CRYPTO_HPP
#define SNOWFLAKECLIENT_CRYPTO_HPP

#include "CryptoTypes.hpp"
#include "CipherContext.hpp"
#include "HashContext.hpp"
#include <memory>
#include <openssl/evp.h>

namespace Snowflake
{
namespace Client
{
namespace Crypto
{
/**
 * Facade over our cryptographic library of choice (abstracted away).
 */
class Cryptor
{
public:
  static Cryptor &getInstance()
  {
    static Cryptor cryptor;
    return cryptor;
  }

  /**
   * Generate a random key.
   *
   * @param(OUT) key
   *    Generated key.
   * @param nbBits
   *    Number of key bits: 128, 192, or 256.
   * @param dev
   *    Random device to use.
   */
  static void generateKey(CryptoKey &key,
                          size_t nbBits,
                          CryptoRandomDevice dev);

  /**
   * Generate a random initialization vector.
   *
   * @param(OUT) iv
   *    Generated initialization vector.
   * @param dev
   *    Random device to use.
   */
  static void generateIV(CryptoIV &iv,
                         CryptoRandomDevice dev);

  /**
   * Create encryption/decryption context.
   *
   * @param algo
   *    Encryption algorithm.
   * @param mode
   *    Encryption mode.
   * @param padding
   *    Padding mode. If 'NONE' is chosen, the client is responsible for making
   *    the input of encryption/decryption a multiple of the cipher block size.
   * @param key
   *    Key to use. Callee makes a copy.
   * @param iv
   *    Initialization vector to use. Callee makes a copy.
   * @param externalDecryptErrors
   *    Treat decryption errors as external errors (throws external exception
   *    instead of internal exception).
   * @return
   *    New encryption/decryption context.
   */
  CipherContext createCipherContext(CryptoAlgo algo,
                                    CryptoMode mode,
                                    CryptoPadding padding,
                                    const CryptoKey &key,
                                    const CryptoIV &iv,
                                    bool externalDecryptErrors = false);

  /**
   * Create encryption/decryption context.
   *
   * @param algo
   *    Encryption algorithm.
   * @param mode
   *    Encryption mode.
   * @param padding
   *    Padding mode. If 'NONE' is chosen, the client is responsible for making
   *    the input of encryption/decryption a multiple of the cipher block size.
   * @param key
   *    Shared key to use. Callee promises not to modify the key.
   * @param iv
   *    Initialization vector to use. Callee makes a copy.
   * @param externalDecryptErrors
   *    Treat decryption errors as external errors (throws external exception
   *    instead of internal exception).
   * @return
   *    New encryption/decryption context.
   */
  CipherContext createCipherContext(CryptoAlgo algo,
                                    CryptoMode mode,
                                    CryptoPadding padding,
                                    const ::std::shared_ptr<const CryptoKey> &key,
                                    const CryptoIV &iv,
                                    bool externalDecryptErrors = false);

/**
   * Create hash context.
   *
   * @param func
   *    Cryptographic hash function.
   * @return
   *    New hash context.
   */
  HashContext createHashContext(CryptoHashFunc func);

private:
  Cryptor();

  ~Cryptor();

  Cryptor(Cryptor const &);

  void operator=(Cryptor const &);
};
}
}
}
#endif //SNOWFLAKECLIENT_CRYPTO_HPP
