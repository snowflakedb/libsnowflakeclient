/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "CipherContext.hpp"

#include <cstring>
#include <algorithm>

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <assert.h>
#include <iostream>

using namespace std;
using namespace Snowflake::Client::Crypto;

namespace Snowflake
{
namespace Client
{
namespace Crypto
{

static_assert(
  AES_BLOCK_SIZE == cryptoAlgoBlockSize(CryptoAlgo::AES),
  "OpenSSL disagrees on AES block size");

static const char *const EXTERNAL_DECRYPT_ERROR_MSG =
  "Failed to decrypt. Check file key and master key.";

/**
 * Get cipher for a given algorithm and mode combination. Throws assertion
 * if desired combination does not exist.
 *
 * Defined in Crypto.cpp
 */
extern const EVP_CIPHER *getCipher(CryptoAlgo algo,
                                   size_t nbBits,
                                   CryptoMode mode);

/**
 * Cipher context implementation class.
 */
struct CipherContext::Impl
{
  Impl(const CryptoAlgo algo,
       const CryptoMode mode,
       const CryptoPadding padding,
       const shared_ptr<const CryptoKey> &key,
       const CryptoIV &iv,
       bool externalDecryptErrors);

  ~Impl() noexcept;

  /// Current operation.
  CryptoOperation op;

  /// Current input and output offsets.
  size_t inOff, outOff;

  /// Wrapped library context.
  EVP_CIPHER_CTX *ctx;

  /// Chosen cipher.
  const EVP_CIPHER *const cipher;

  /// Encryption algorithm.
  const CryptoAlgo algo;

  /// Encryption mode.
  const CryptoMode mode;

  /// Padding mode.
  const CryptoPadding padding;

  /// Encryption key.
  const shared_ptr<const CryptoKey> key;

  /// Original initialization vector.
  const CryptoIV iv;

  /// Whether or not to treat decryption errors as external errors.
  const bool externalDecryptErrors;
};

CipherContext::Impl::Impl(const CryptoAlgo algo,
                          const CryptoMode mode,
                          const CryptoPadding padding,
                          const shared_ptr<const CryptoKey> &key,
                          const CryptoIV &iv,
                          const bool externalDecryptErrors)
  : op(CryptoOperation::INVALID),
    inOff(-1),
    outOff(-1),
    cipher(getCipher(algo, key->nbBits, mode)),
    algo(algo),
    mode(mode),
    padding(padding),
    key(key),
    iv(iv),
    externalDecryptErrors(externalDecryptErrors)
{
  //TODO Make sure library context is erased in case of a core dump.
  //MemoryWiper::registerRegion(&this->ctx, sizeof(this->ctx));

  // Initialize the library context.
  this->ctx = EVP_CIPHER_CTX_new();

  // Set padding.
  bool pad;
  switch (padding)
  {
    case CryptoPadding::NONE:
      pad = false;
      break;
    case CryptoPadding::PKCS5:
      pad = true;
      break;
    default:
      //TODO port assertion framework
      throw;
  }
  //TODO
  if (EVP_CIPHER_CTX_set_padding(this->ctx, pad) != 1)
    //TODO throw exception
    throw;
  //SF_THROW_CRYPTO(pad);
}

CipherContext::Impl::~Impl() noexcept
{
  // Destroy the library context (securely cleans up any keys therein).
  EVP_CIPHER_CTX_free(this->ctx);

  //TODO OpenSSL has already cleaned up the library context.
  //MemoryWiper::unregisterRegion(&this->ctx, false);
}

CipherContext::CipherContext() = default;

CipherContext::CipherContext(const CryptoAlgo algo,
                             const CryptoMode mode,
                             const CryptoPadding padding,
                             const shared_ptr<const CryptoKey> &key,
                             const CryptoIV &iv,
                             const bool externalDecryptErrors)
  : m_pimpl(new Impl(algo, mode, padding, key, iv,
                     externalDecryptErrors))
{
}

CipherContext::CipherContext(
  CipherContext &&other) noexcept = default;

CipherContext::~CipherContext() noexcept = default;

CipherContext &
CipherContext::operator=(CipherContext &&other) noexcept
= default;

void CipherContext::reset() noexcept
{
  m_pimpl.reset();
}

CipherContext CipherContext::clone() const
{
  CipherContext result;
  if (m_pimpl)
  {
    const Impl &impl = *m_pimpl;
    result.m_pimpl.reset(
      new Impl(impl.algo, impl.mode, impl.padding,
               impl.key, impl.iv,
               impl.externalDecryptErrors));
  }
  return result;
}

void CipherContext::getStatus(CryptoOperation &op,
                              size_t &inOff,
                              size_t &outOff) const noexcept
{
  if (m_pimpl)
  {
    op = m_pimpl->op;
    inOff = m_pimpl->inOff;
    outOff = m_pimpl->outOff;
  } else
  {
    op = CryptoOperation::INVALID;
    inOff = outOff = -1;
  }
}

void CipherContext::initialize(const CryptoOperation op,
                               const size_t offset)
{
  // Not valid on default constructed context objects.
  //TODO
  //SF_ASSERT0(m_pimpl, "context_not_valid_1");

  // Add offset to IV if in counter mode. Otherwise, just use as-is.
  auto &impl = *m_pimpl;

  // Perform initialization.
  int success;
  switch (op)
  {
    case CryptoOperation::ENCRYPT:
      success = EVP_EncryptInit_ex(impl.ctx, impl.cipher,
                                   nullptr,
                                   reinterpret_cast<const unsigned char *>(impl.key->data),
                                   reinterpret_cast<const unsigned char *>(impl.iv.data));
      break;
    case CryptoOperation::DECRYPT:
      success = EVP_DecryptInit_ex(impl.ctx, impl.cipher,
                                   nullptr,
                                   reinterpret_cast<const unsigned char *>(impl.key->data),
                                   reinterpret_cast<const unsigned char *>(impl.iv.data));
      break;
    default:
      throw;
      //TODO
      //SF_ASSERT_ALWAYS("invalid_operation",
      //                 static_cast<int>(op));
  }

  // Throw exception on failure.
  // 1 for success and 0 for failure
  if (success == 0)
    throw;
  //SF_THROW_CRYPTO(static_cast<int>(op));

  // Remember operation and offset.
  impl.op = op;
  impl.inOff = impl.outOff = offset;
}

size_t CipherContext::next(void *const out,
                           const void *const in,
                           const size_t len)
{
  // Not valid on default constructed context objects.
  //SF_ASSERT0(m_pimpl, "context_not_valid_2");

  // Continue current operation.
  auto &impl = *m_pimpl;
  int success = 0;
  int nbBytesOut;
  switch (impl.op)
  {
    case CryptoOperation::ENCRYPT:
      // Encrypt next batch of input data.
      success = EVP_EncryptUpdate(impl.ctx,
                                  static_cast<unsigned char *>(out),
                                  &nbBytesOut,
                                  static_cast<const unsigned char *>(in),
                                  (int)len);
      break;

    case CryptoOperation::DECRYPT:
      success = EVP_DecryptUpdate(impl.ctx,
                                  static_cast<unsigned char *>(out),
                                  &nbBytesOut,
                                  static_cast<const unsigned char *>(in),
                                  (int)len);
      break;

    default:
      //TODO
      throw;
      //SF_ASSERT_ALWAYS0("not_initialized_1");
  }

  // Throw exception on failure.
  if (success != 1)
  {
    if (impl.externalDecryptErrors &&
        (impl.op == CryptoOperation::DECRYPT))
      //TODO throw
      throw;
      //SF_THROW_MSG(Error::ErrorId::ERR_CRYPTO_CALL_FAILED,
      //             EXTERNAL_DECRYPT_ERROR_MSG);
    else
      throw;
    //SF_THROW_CRYPTO(static_cast<int>(impl.op));
  }

  // Update offsets.
  impl.inOff += len;
  impl.outOff += nbBytesOut;

  // Return number of bytes written.
  return nbBytesOut;
}

size_t CipherContext::finalize(void *const out)
{
  // Not valid on default constructed context objects.
  //SF_ASSERT0(m_pimpl, "context_not_valid_3");

  // Finalize current operation.
  auto &impl = *m_pimpl;
  int success;
  int nbBytesOut;
  switch (impl.op)
  {
    case CryptoOperation::ENCRYPT:
      // Encrypt final batch of input data.
      success = EVP_EncryptFinal_ex(impl.ctx,
                                    static_cast<unsigned char *>(out),
                                    &nbBytesOut);
      break;

    case CryptoOperation::DECRYPT:
    {
      // Decrypt final batch of input data.
      success = EVP_DecryptFinal_ex(impl.ctx,
                                    static_cast<unsigned char *>(out),
                                    &nbBytesOut);

      break;
    }
    default:
      throw;
      //SF_ASSERT_ALWAYS0("not_initialized_2");
  }

  // Throw exception on failure.
  if (success != 1)
  {
    if (impl.externalDecryptErrors &&
        (impl.op == CryptoOperation::DECRYPT))
      //TODO
      throw;
      //SF_THROW_MSG(Error::ErrorId::ERR_CRYPTO_CALL_FAILED,
      //             EXTERNAL_DECRYPT_ERROR_MSG);
    else
    {
      printf("Finialize failed, op %d", static_cast<int>(impl.op));
      throw;
    }
    //SF_THROW_CRYPTO(static_cast<int>(impl.op));
  }

  // Reset current operation and offset.
  impl.op = CryptoOperation::INVALID;
  impl.inOff = impl.outOff = -1;

  // Return number of bytes written.
  return nbBytesOut;
}
}
}
}