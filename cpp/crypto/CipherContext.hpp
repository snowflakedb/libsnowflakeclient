/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_CIPHERCONTEXT_HPP
#define SNOWFLAKECLIENT_CIPHERCONTEXT_HPP

#include "CryptoTypes.hpp"
#include "CipherContext.hpp"
#include <cstring>
#include <memory>

namespace Snowflake
{
namespace Client
{
namespace Crypto
{
/**
 * Encryption/decryption context.
 *
 * Context objects behave like std::unique_ptrs. They cannot be copied, but
 * they can be moved. On destruction, a context object will release any
 * underlying library resources it owns.
 */
class CipherContext
{
public:

  /**
   * Default constructor.
   *
   * Context objects in the default-constructed state cannot be used. They
   * need to be move-assigned from another valid context object first.
   */
  CipherContext();


  /**
   * Destructor.
   *
   * Frees any underlying library resources. It is okay to have an unfinished
   * operation (but it will be impossible to continue with that operation).
   */
  ~CipherContext() noexcept;

  /**
   * Move constructor.
   *
   * Leaves the other context in an undefined but destructible state.
   */
  CipherContext(CipherContext &&other) noexcept;

  /**
   * Move-assignment operator.
   *
   * Leaves the other context in an undefined but destructible state.
   */
  CipherContext &operator=(CipherContext &&other) noexcept;

  void initialize_encryption() const;

  void initialize_decryption() const;

  void set_padding() const;

  void initialize_gcm(CryptoOperation op, size_t offset) const;

  size_t gcm_encrypt(unsigned char *ciphertext, unsigned char const *plaintext, size_t plaintext_len) const;

  size_t gcm_decrypt(unsigned char *plaintext, unsigned char const *ciphertext, size_t ciphertext_len) const;

  size_t finalize_encryption(unsigned char *ciphertext, int len, unsigned char *tag) const;

  size_t finalize_decryption(unsigned char *plaintext, int len, void *tag) const;

  size_t next_gcm(void *out, const void *in, size_t len) const;

  size_t finalize_gcm(void *out, unsigned char *tag) const;

  /**
   * Whether or not this is a valid context that can be used for encryption
   * or decryption. Returns 'false' for default constructed context object or
   * context objects that have been the source of a move construction or
   * move assignment.
   */
  inline bool isValid() const noexcept;

  /**
   * Swap state of two contexts.
   */
  inline void swap(CipherContext &other) noexcept;

  /**
   * Initialize a new encryption or decryption operation. Must be called before
   * any call to next() or finalize().
   *
   * If there is already an active operation, that operation is finalized
   * and any padding bytes that finalization emits are discarded.
   *
   * @param op
   *    Encryption or decryption.
   * @param offset
   *    Offset into data stream. Valid only for CTR mode, otherwise must be 0.
   *    Must be a multiple of the size of the initialization vector (i.e. the
   *    cipher block size).
   */
  void initialize(CryptoOperation op,
                  size_t offset = 0);

  /**
   * Encrypt/decrypt another block of data. May be called multiple times
   * between a matching pair of calls to initialize() and finalize().
   *
   * WARNING: If padding is enabled, the output memory must be at least
   * 'len' plus the cipher block size of the encryption algorithm.
   *
   * @param out
   *    Output block.
   * @param in
   *    Input block.
   * @param len
   *    Size of input block in number of bytes.
   * @return
   *    Number of bytes written to output block.
   */
  size_t next(void *out,
              const void *in,
              size_t len);

  /**
   * Finalize current operation. After finalize() has been called, another
   * operation can be started by calling initialize().
   *
   * WARNING: If padding is enabled, the output memory must be at least
   * 'len' plus the cipher block size of the encryption algorithm.
   *
   * @param out
   *    Output block.
   * @return
   *    Number of bytes written to output block.
   */
  size_t finalize(void *out);

private:

  friend class Cryptor;

  /**
   * Constructor for creating a valid context. Can only be called by the
   * Crypto facade. Ensures that the crypto library is initialized before
   * context objects are used.
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
   */
  CipherContext(CryptoAlgo algo,
                CryptoMode mode,
                CryptoPadding padding,
                const std::shared_ptr<const CryptoKey> &key,
                const CryptoIV &iv,
                bool externalDecryptErrors);

  /**
   * Cipher context implementation class.
   */
  struct Impl;

  /// Pointer to implementation ("pimpl").
  std::unique_ptr<Impl> m_pimpl;

};

inline bool CipherContext::isValid() const noexcept
{
  return !!m_pimpl;
}

inline void CipherContext::swap(CipherContext &other) noexcept
{
  m_pimpl.swap(other.m_pimpl);
}

/**
 * Overload of std::swap for CipherContexts.
 */
inline void swap(CipherContext &left,
                 CipherContext &right) noexcept
{
  left.swap(right);
}

}
}
}
#endif //SNOWFLAKECLIENT_CIPHERCONTEXT_HPP
