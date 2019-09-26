/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_CRYPTOTYPES_HPP
#define SNOWFLAKECLIENT_CRYPTOTYPES_HPP

#include <cstring>
#include "snowflake/Simba_CRTFunctionSafe.h"

namespace Snowflake
{
namespace Client
{
namespace Crypto
{

/// Initialization vector width (in bits).
#define SF_CRYPTO_IV_NBITS 128

#define SF_CRYPTO_CONCAT_PASTE(x, y) x ## _ ## y
#define SF_CRYPTO_CONCAT(x, y) SF_CRYPTO_CONCAT_PASTE(x, y)

/// Cipher of choice, defined as the triplet (algo, nbits, mode).
#define SF_CRYPTO_CIPHER(algo, nbits, mode) \
SF_CRYPTO_CONCAT(SF_CRYPTO_CONCAT(SF_CRYPTO_CONCAT(EVP, algo), \
                            nbits), \
           mode)

/**
* Encryption algorithm.
*/
enum class CryptoAlgo
{
  AES   /// Advanced Encryption Standard
};

/**
* Encryption mode.
*/
enum class CryptoMode
{
  CBC,  /// Cipher-block chaining
  CFB,  /// Cipher feedback
  //CTR,  /// Counter NOT SUPPORTED
  ECB,  /// Electronic codebook
  GCM,  /// Galois counter
  OFB,  /// Output feedback
  KW    /// Key wrap
};

/**
* Padding mode.
*/
enum class CryptoPadding
{
  NONE, /// No padding. Client must ensure stream is a multiple of block size.
  PKCS5 /// PKCS#5 padding.
};

/**
* Encryption key.
*
* CryptoKey structures are automatically wiped before core dumps.
*/
struct CryptoKey final
{
  inline CryptoKey()
  {
    //TODO Disable Memory wiper for now
    // MemoryWiper::registerRegion(this, sizeof(*this));
  }

  inline CryptoKey(const CryptoKey &other) : CryptoKey()
  {
    nbBits = other.nbBits;
    sb_memcpy(data, sizeof(data), other.data, sizeof(data));
  }

  inline ~CryptoKey() noexcept
  {
    //TODO Disable Memory wiper for now
    // MemoryWiper::unregisterRegion(this, true);
  }

  size_t nbBits;      /// 128, 192, or 256
  char data[
    256 / 8]; /// between 128 and 256 bits of key data, @see nbBits
};

/**
* Initialization vector.
*/
struct CryptoIV final
{
  inline CryptoIV(){}

  char data[SF_CRYPTO_IV_NBITS / 8];
};

/**
* Cryptographic hash function.
*/
enum class CryptoHashFunc
{
  MD5,
  SHA1,
  SHA224,
  SHA256,
  SHA384,
  SHA512
};

/**
* Available random devices.
*/
enum class CryptoRandomDevice
{
  DEV_RANDOM,   // /dev/random    - slow but very secure
  DEV_URANDOM,  // /dev/urandom   - fast but pseudo-random
  //CTR_DRBG      // CTR-DRBG seeded from /dev/urandom NOT SUPPORTED
};

/**
* Encrypt or decrypt operation flag.
*/
enum class CryptoOperation
{
  ENCRYPT,
  DECRYPT,
  INVALID
};

/**
* Get cipher block size, in number of bytes, for a given encryption algorithm.
*
* @param algo
*    Encryption algorithm.
* @return
*    Cipher block size, in number of bytes.
*/
inline constexpr size_t cryptoAlgoBlockSize(CryptoAlgo algo) noexcept
{
  return algo == CryptoAlgo::AES ? 16 : 0;
}

/**
 * Get message digest size, in number of bytes, for a given cryptographic
 * hash function.
 *
 * @param func
 *    Cryptographic hash function.
 * @return
 *    Message digest size, in number of bytes.
 */
inline constexpr size_t cryptoHashDigestSize(CryptoHashFunc func) noexcept
{
  return func == CryptoHashFunc::MD5 ? 16 :
         func == CryptoHashFunc::SHA1 ? 20 :
         func == CryptoHashFunc::SHA224 ? 28 :
         func == CryptoHashFunc::SHA256 ? 32 :
         func == CryptoHashFunc::SHA384 ? 48 :
         func == CryptoHashFunc::SHA512 ? 64 : 0;
}
}
}
}

#endif //SNOWFLAKECLIENT_CRYPTOTYPES_HPP
