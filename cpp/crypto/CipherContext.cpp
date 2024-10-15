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
#include <openssl/err.h>

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

void CipherContext::initialize_encryption() const {
  switch (m_pimpl->mode) {
    case CryptoMode::GCM:
      // initialize encryption operation
      if (1 != EVP_EncryptInit_ex(m_pimpl->ctx, m_pimpl->cipher, nullptr, nullptr, nullptr)) {
        throw;
      }

      // default iv len for AES is 96 bits but driver's IV generator
      // uses 128 bits defined as SF_CRYPTO_IV_NBITS
      if (1 != EVP_CIPHER_CTX_ctrl(m_pimpl->ctx, EVP_CTRL_GCM_SET_IVLEN, SF_CRYPTO_IV_NBITS / 8, nullptr)) {
        throw;
      }

      // initialize key and iv
      if (1 != EVP_EncryptInit_ex(m_pimpl->ctx, nullptr, nullptr,
        reinterpret_cast<const unsigned char *>(m_pimpl->key->data),
        reinterpret_cast<const unsigned char *>(m_pimpl->iv.data))) {
        throw;
      }

    break;
    default:
      if (1 != EVP_EncryptInit_ex(m_pimpl->ctx, m_pimpl->cipher, nullptr,
         reinterpret_cast<const unsigned char *>(m_pimpl->key->data),
         reinterpret_cast<const unsigned char *>(m_pimpl->iv.data))) {
        throw;
      }
    break;
  }
}

void CipherContext::initialize_decryption() const {
  switch (m_pimpl->mode) {
    case CryptoMode::GCM:
      if (1 != EVP_DecryptInit_ex(m_pimpl->ctx, m_pimpl->cipher, nullptr, nullptr, nullptr)) {
        throw;
      }

      if (1 != EVP_CIPHER_CTX_ctrl(m_pimpl->ctx, EVP_CTRL_GCM_SET_IVLEN, SF_CRYPTO_IV_NBITS / 8, nullptr)) {
        throw;
      }

      if (1 != EVP_DecryptInit_ex(m_pimpl->ctx, nullptr, nullptr,
        reinterpret_cast<const unsigned char *>(m_pimpl->key->data),
        reinterpret_cast<const unsigned char *>(m_pimpl->iv.data))) {
        throw;
      }
    break;
    default:
      if (1 != EVP_DecryptInit_ex(m_pimpl->ctx, m_pimpl->cipher, nullptr,
         reinterpret_cast<const unsigned char *>(m_pimpl->key->data),
         reinterpret_cast<const unsigned char *>(m_pimpl->iv.data)))
    break;
  }
}



void CipherContext::set_padding() const {
  switch (m_pimpl->padding) {
    case CryptoPadding::NONE:
      break;
    case CryptoPadding::PKCS5:
      if (1 != EVP_CIPHER_CTX_set_padding(m_pimpl->ctx, true)) {
        throw;
      }
    break;
  }
}

void CipherContext::initialize_gcm(const CryptoOperation op, const size_t offset) const {
  switch (op) {
    case CryptoOperation::ENCRYPT:
      initialize_encryption();
    break;
    case CryptoOperation::DECRYPT:
      initialize_decryption();
    break;
    default:
      throw;
  }
  set_padding();
  m_pimpl->op = op;
  m_pimpl->inOff = m_pimpl->outOff = offset;
}

size_t CipherContext::gcm_encrypt(unsigned char *ciphertext, unsigned char const *const plaintext,  size_t const plaintext_len) const {
  int len;
  /*
 * Provide the message to be encrypted, and obtain the encrypted output.
 * EVP_EncryptUpdate can be called multiple times if necessary
 */
  if (1 != EVP_EncryptUpdate(m_pimpl->ctx, ciphertext, &len, plaintext, static_cast<int>(plaintext_len))) {
    throw;
  }
  return len;
}

size_t CipherContext::gcm_decrypt(unsigned char *plaintext, unsigned char const *const ciphertext, size_t const ciphertext_len) const {
  int len;
  if (1 != EVP_DecryptUpdate(m_pimpl->ctx, plaintext, &len, ciphertext, static_cast<int>(ciphertext_len))) {
    throw;
  }

  return len;
}

size_t CipherContext::finalize_encryption(unsigned char *ciphertext, int len, unsigned char *tag) const {
  int ciphertext_len = len;
  /*
  * Finalise the encryption. Normally ciphertext bytes may be written at
  * this stage, but this does not occur in GCM mode
  */
  if (1 != EVP_EncryptFinal_ex(m_pimpl->ctx, ciphertext + ciphertext_len, &len)) {
    throw;
  }
  ciphertext_len += len;

  if (1 != EVP_CIPHER_CTX_ctrl(m_pimpl->ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
    throw;
  }

  return ciphertext_len;
}

size_t CipherContext::finalize_decryption(unsigned char *plaintext, int len, void* tag) const {
  int plaintext_len = len;
  int nbBytesOut;
  if (1 != EVP_CIPHER_CTX_ctrl(m_pimpl->ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)) {
    throw;
  }

  const int ret = EVP_DecryptFinal_ex(m_pimpl->ctx, plaintext, &nbBytesOut);
  if (ret > 0) {
    plaintext_len += nbBytesOut;
    return plaintext_len;
  }
  return 0;
}

size_t CipherContext::next_gcm(void *const out,
                               void const *const in,
                               const size_t len) const {
  size_t res = 0;
  switch (m_pimpl->op) {
    case CryptoOperation::ENCRYPT:
      res = gcm_encrypt(static_cast<unsigned char *>(out),
                        static_cast<unsigned char const *>(in), len);
    break;
    case CryptoOperation::DECRYPT:
      res = gcm_decrypt(static_cast<unsigned char *>(out),
                        static_cast<unsigned char const *>(in), len);
    break;
    default:
      throw;
  }
  m_pimpl->inOff += len;
  m_pimpl->outOff += res;

  return res;
}

size_t CipherContext::finalize_gcm(void *const out, unsigned char *tag) const {
  size_t success;
  switch (m_pimpl->op) {
    case CryptoOperation::ENCRYPT:
      success = finalize_encryption(static_cast<unsigned char *>(out), static_cast<int>(m_pimpl->inOff), tag);
    break;
    case CryptoOperation::DECRYPT:
      success = finalize_decryption(static_cast<unsigned char *>(out), static_cast<int>(m_pimpl->inOff), tag);
    break;
    default:
      throw;
  }

  return success;
}

void CipherContext::initialize(const CryptoOperation op,
                               const size_t offset)
{
  // Not valid on default constructed context objects.
  //TODO
  //SF_ASSERT0(m_pimpl, "context_not_valid_1");

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


  // Set padding.
  // EVP_CIPHER_CTX_set_padding() must be called after initialize
  // https://www.openssl.org/docs/manmaster/man3/EVP_CIPHER_CTX_set_padding.html
  bool pad;
  switch (impl.padding)
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
  if (EVP_CIPHER_CTX_set_padding(impl.ctx, pad) != 1)
    //TODO throw exception
    throw;
  //SF_THROW_CRYPTO(pad);

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
      sf_printf("Finialize failed, op %d", static_cast<int>(impl.op));
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