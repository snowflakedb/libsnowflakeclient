#include "CipherContext.hpp"

#include <algorithm>

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <iostream>

#include "../logger/SFLogger.hpp"

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

  /// Whether to treat decryption errors as external errors.
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
  : m_pimpl{std::make_unique<Impl>(algo, mode, padding, key, iv,
                     externalDecryptErrors)}
{
}

CipherContext::CipherContext(
  CipherContext &&other) noexcept = default;

CipherContext::~CipherContext() noexcept = default;

CipherContext &
CipherContext::operator=(CipherContext &&other) noexcept
= default;

void CipherContext::initialize_encryption() {
  CXX_LOG_DEBUG("Initialize encryption, mode = %d", m_pimpl->mode);
  switch (m_pimpl->mode) {
    case CryptoMode::GCM:
      // initialize encryption operation
      if (1 != EVP_EncryptInit_ex(m_pimpl->ctx, m_pimpl->cipher, nullptr, nullptr, nullptr)) {
        CXX_LOG_ERROR("Failed to initialize encryption operation for provided cipher");
        throw;
      }

      // initialize key and iv
      if (1 != EVP_EncryptInit_ex(m_pimpl->ctx, nullptr, nullptr,
        reinterpret_cast<const unsigned char *>(m_pimpl->key->data),
        reinterpret_cast<const unsigned char *>(m_pimpl->iv.data))) {
        CXX_LOG_ERROR("Failed to initialize encryption key and iv")
        throw;
      }
    	break;
    default:
      if (1 != EVP_EncryptInit_ex(m_pimpl->ctx, m_pimpl->cipher, nullptr,
         reinterpret_cast<const unsigned char *>(m_pimpl->key->data),
         reinterpret_cast<const unsigned char *>(m_pimpl->iv.data))) {
        CXX_LOG_ERROR("Failed to initialize encryption operation with key and iv")
        throw;
      }
    	break;
  }
}

void CipherContext::initialize_decryption() {
  CXX_LOG_DEBUG("Initializing decryption, mode = %d", m_pimpl->mode);
  switch (m_pimpl->mode) {
    case CryptoMode::GCM:
      if (1 != EVP_DecryptInit_ex(m_pimpl->ctx, m_pimpl->cipher, nullptr, nullptr, nullptr)) {
        CXX_LOG_ERROR("Failed to initialize decryption operation for provided cipher");
        throw;
      }

      if (1 != EVP_DecryptInit_ex(m_pimpl->ctx, nullptr, nullptr,
        reinterpret_cast<const unsigned char *>(m_pimpl->key->data),
        reinterpret_cast<const unsigned char *>(m_pimpl->iv.data))) {
        CXX_LOG_ERROR("Failed to initialize decryption key and iv")
        throw;
      }
    	break;
    default:
      if (1 != EVP_DecryptInit_ex(m_pimpl->ctx, m_pimpl->cipher, nullptr,
         reinterpret_cast<const unsigned char *>(m_pimpl->key->data),
         reinterpret_cast<const unsigned char *>(m_pimpl->iv.data))) {
        CXX_LOG_ERROR("Failed to initialize decryption operation with key and iv")
        throw;
      }
    	break;
  }
}



void CipherContext::set_padding() {
  // EVP_CIPHER_CTX_set_padding() must be called after initialize
  // https://www.openssl.org/docs/manmaster/man3/EVP_CIPHER_CTX_set_padding.html
  switch (m_pimpl->padding) {
    case CryptoPadding::NONE:
      break;
    case CryptoPadding::PKCS5:
      if (1 != EVP_CIPHER_CTX_set_padding(m_pimpl->ctx, true)) {
        CXX_LOG_ERROR("Failed to set padding to PKCS5")
        throw;
      }
    break;
  }
}

void CipherContext::set_aad(const unsigned char *aad, int aad_len) {
  if (m_pimpl->mode != CryptoMode::GCM) {
    CXX_LOG_ERROR("AAD is not supported outside GCM mode");
    throw;
  }
  switch (m_pimpl->op) {
    case CryptoOperation::ENCRYPT:
      if(1 != EVP_EncryptUpdate(m_pimpl->ctx, nullptr, nullptr, aad, aad_len)) {
        CXX_LOG_ERROR("Failed to set the additional authenticated data for encryption")
        throw;
      }
      break;
    case CryptoOperation::DECRYPT:
      if(1 != EVP_DecryptUpdate(m_pimpl->ctx, nullptr, nullptr, aad, aad_len)) {
        CXX_LOG_ERROR("Failed to set the additional authenticated data for decryption")
        throw;
      }
    	break;
    default:
      CXX_LOG_ERROR("Failed to set the additional authenticated data")
      throw;
  }
}

void CipherContext::initialize(const CryptoOperation op, const unsigned char *aad, const int aad_len) {
  initialize(op);
  set_aad(aad, aad_len);
}

void CipherContext::initialize(const CryptoOperation op,
                               const size_t offset) {
  // Not valid on default constructed context objects.
  //TODO
  //SF_ASSERT0(m_pimpl, "context_not_valid_1");

  switch (op)
  {
    case CryptoOperation::ENCRYPT:
      initialize_encryption();
      break;
    case CryptoOperation::DECRYPT:
      initialize_decryption();
      break;
    default:
      CXX_LOG_ERROR("CipherContext::initialize() failed, operation %s not supported", op);
      throw;
  }

  set_padding();
  m_pimpl->op = op;
  m_pimpl->inOff = m_pimpl->outOff = offset;
}

size_t CipherContext::next(void *const out,
                           const void *const in,
                           const size_t len) {
  // Not valid on default constructed context objects.
  //SF_ASSERT0(m_pimpl, "context_not_valid_2");

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
                                  static_cast<int>(len));
      break;
    case CryptoOperation::DECRYPT:
      success = EVP_DecryptUpdate(impl.ctx,
                                  static_cast<unsigned char *>(out),
                                  &nbBytesOut,
                                  static_cast<const unsigned char *>(in),
                                  static_cast<int>(len));
      break;
    default:
      CXX_LOG_ERROR("CipherContext::next() failed due to an illegal operation. Probably it wasn't initialized properly.");
      //TODO
      throw;
      //SF_ASSERT_ALWAYS0("not_initialized_1");
  }

  // Throw exception on failure.
  if (success != 1)
  {
    if (impl.externalDecryptErrors && impl.op == CryptoOperation::DECRYPT) {
      //TODO throw
      //SF_THROW_MSG(Error::ErrorId::ERR_CRYPTO_CALL_FAILED,
      //             EXTERNAL_DECRYPT_ERROR_MSG);
      CXX_LOG_ERROR("CipherContext::next() failed with external decrypt errors");
      throw;
    }

    CXX_LOG_ERROR("CipherContext::next() failed");
    throw;
    //SF_THROW_CRYPTO(static_cast<int>(impl.op));
  }

  // Update offsets.
  impl.inOff += len;
  impl.outOff += nbBytesOut;

  // Return number of bytes written.
  return nbBytesOut;
}

size_t CipherContext::finalize(void *const out, unsigned char *tag) {
  if (m_pimpl->mode != CryptoMode::GCM) {
    CXX_LOG_ERROR("CipherContext::finalize() with tag must be executed in GCM mode");
    throw;
  }
  // Not valid on default constructed context objects.
  //SF_ASSERT0(m_pimpl, "context_not_valid_3");

  // Finalize current operation.
  int nbBytesOut;
  switch (m_pimpl->op)
  {
    case CryptoOperation::ENCRYPT:
      /*
      * Finalise the encryption. Normally ciphertext bytes may be written at
      * this stage, but this does not occur in GCM mode
      */
      if (1 != EVP_EncryptFinal_ex(m_pimpl->ctx, static_cast<unsigned char *>(out), &nbBytesOut)) {
        CXX_LOG_ERROR("CipherContext::finalize() failed finalizing the encryption");
        throw;
      }

      if (1 != EVP_CIPHER_CTX_ctrl(m_pimpl->ctx, EVP_CTRL_GCM_GET_TAG, SF_GCM_TAG_LEN, tag)) {
        CXX_LOG_ERROR("CipherContext::finalize() failed getting the GCM tag");
        throw;
      }
    	break;
    case CryptoOperation::DECRYPT:
      // Decrypt final batch of input data.
      if (1 != EVP_CIPHER_CTX_ctrl(m_pimpl->ctx, EVP_CTRL_GCM_SET_TAG, SF_GCM_TAG_LEN, tag)) {
        CXX_LOG_ERROR("CipherContext::finalize() failed setting the GCM tag");
        throw;
      }

      if (1 !=  EVP_DecryptFinal_ex(m_pimpl->ctx, static_cast<unsigned char *>(out), &nbBytesOut)) {
        if (m_pimpl->externalDecryptErrors) {
          CXX_LOG_ERROR("CipherContext::finalize() failed with external decrypt errors");
          throw;
        }
        CXX_LOG_ERROR("CipherContext::finalize() failed finalizing the decryption");
        throw;
      }
      break;
    default:
      CXX_LOG_ERROR("CipherContext::finalize() failed, unsupported operation, %d", static_cast<int>(m_pimpl->op));
    throw;
  }
  nbBytesOut += static_cast<int>(m_pimpl->inOff);

  // Reset current operation and offset.
  m_pimpl->op = CryptoOperation::INVALID;
  m_pimpl->inOff = m_pimpl->outOff = -1;

  // Return number of bytes written.
  return nbBytesOut;
}

size_t CipherContext::finalize(void *const out) {
  if (m_pimpl->mode == CryptoMode::GCM) {
    CXX_LOG_ERROR("CipherContext::finalize(void *cons out) was called with GCM mode while finalize with tag parameter should be called in this mode");
    throw;
  }
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
      CXX_LOG_ERROR("CipherContext::finalize() failed, unsupported operation");
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