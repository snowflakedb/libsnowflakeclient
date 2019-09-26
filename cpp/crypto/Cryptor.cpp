/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "Cryptor.hpp"

#include <cassert>
#include <cstring>
#include <new>
#include <random>
#include <utility>
#include <vector>
#include <string>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "CipherContext.hpp"
#include "snowflake/platform.h"

namespace Snowflake
{
namespace Client
{
namespace Crypto
{

// Static vector of mutexes for use by OpenSSL.
//typedef ::std::pair<SF_MUTEX_HANDLE, bool> MutexSlot;
//typedef ::std::vector<MutexSlot> MutexVector;
//static MutexVector *s_mutexes = nullptr;

/**
 * Get some random bytes from the file system.
 *
 * @param out
 *    Output buffer. Must be of sufficient size.
 * @param nbBytes
 *    How many bytes.
 * @param dev
 *    File to get bytes from.
 */
static void getRandomBytesFromFile(char *const out,
                                   const size_t nbBytes,
                                   const CryptoRandomDevice dev)
{
  // Map random device enum to string.
  static const ::std::string DEV_RANDOM_STR("/dev/random");
  static const ::std::string DEV_URANDOM_STR("/dev/urandom");
  const ::std::string &token = dev == CryptoRandomDevice::DEV_RANDOM
                               ? DEV_RANDOM_STR
                               : DEV_URANDOM_STR;

  // Random value type.
  typedef ::std::random_device::result_type T;
  static constexpr size_t STEP = sizeof(T);
  const size_t steps = nbBytes / STEP;
  const size_t rem = nbBytes % STEP;

  // Create random device.
  ::std::random_device rd(token);

  // Generate required number of random values.
  char *outT = out;
  for (size_t i = 0; i < steps; ++i)
  {
    const T rdv = rd();
    sb_memcpy(outT, nbBytes - (outT-out), &rdv, STEP);
    outT += STEP;
  }

  // Generate and truncate one extra value if number of output types is not
  // an exact multiple of the random value type.
  if (rem)
  {
    const T remVal = rd();
    sb_memcpy(outT, nbBytes - (outT - out), &remVal, rem);
  }
}

/**
 * Get some random bytes from a random device.
 *
 * @param out
 *    Output buffer. Must be of sufficient size.
 * @param nbBytes
 *    How many bytes.
 * @param dev
 *    Random device to get bytes from.
 */
static void getRandomBytes(char *const out,
                           const size_t nbBytes,
                           const CryptoRandomDevice dev)
{
  getRandomBytesFromFile(out, nbBytes, dev);
}

/**
 * Callback for locking/unlocking one of static mutexes.
 */
/*static void lock(const int mode,
                 const int type,
                 const char *const file,
                 const int line) noexcept
{
  // Lazily create mutex on first access.
  assert(static_cast<size_t>(type) < s_mutexes->size());
  MutexSlot &slot = (*s_mutexes)[type];
  int failure = 0;
  if (!slot.second)
  {
    failure = _mutex_init(&slot.first);
    assert(!failure);
    slot.second = true;
  }

  // Lock or unlock the mutex.
  if (mode & CRYPTO_LOCK)
    failure = _mutex_lock(&slot.first);
  else if (mode & CRYPTO_UNLOCK)
    failure = _mutex_unlock(&slot.first);
  assert(!failure);
  (void) failure;
}*/

/**
 * Callback for getting the calling thread's id.
 */
static void getThreadId(CRYPTO_THREADID *const threadId) noexcept
{
  //static_assert(sizeof(pthread_self()) <= sizeof(threadId->val),
  //              "Unable to store thread id in CRYPTO_THREADID");

  CRYPTO_THREADID_set_pointer(theardId, nullptr);
  CRYPTO_THREADID_set_numeric(threadId, pthread_self());
}

/**
 * Callback for dynamically creating a mutex.
 */
//TODO figure out whether locking is required for openssl
/*static CRYPTO_dynlock_value *dynlockCreate(const char *const file,
                                           const int line) noexcept {
    SF_MUTEX_HANDLE *const mutex = new SF_MUTEX_HANDLE;
    assert(mutex);
    const int failure = _mutex_init(mutex);
    assert(!failure);
    (void) failure;
    return reinterpret_cast<CRYPTO_dynlock_value *>(mutex);
}*/

/**
 * Callback for destroying a dynamically created mutex.
 */
/*static void dynlockDestroy(CRYPTO_dynlock_value *const dynlock,
                           const char *const file,
                           const int line) noexcept {
    pthread_mutex_t *const mutex = reinterpret_cast<pthread_mutex_t *>(dynlock);
    const int failure = pthread_mutex_destroy(mutex);
    assert(!failure);
    (void) failure;
    delete mutex;
}*/

/**
 * Callback for locking/unlocking a dynamically created mutex.
 */
/*static void dynlockLock(const int mode,
                        CRYPTO_dynlock_value *const dynlock,
                        const char *const file,
                        const int line) noexcept {
    pthread_mutex_t *const mutex = reinterpret_cast<pthread_mutex_t *>(dynlock);
    int failure = 0;
    if (mode & CRYPTO_LOCK)
        failure = pthread_mutex_lock(mutex);
    else if (mode & CRYPTO_UNLOCK)
        failure = pthread_mutex_unlock(mutex);
    assert(!failure);
    (void) failure;
}*/
/**
 * Get digest routing for a given cryptographic hash function.
 */
const EVP_MD *getDigest(const CryptoHashFunc func)
{
  switch (func)
  {
    case CryptoHashFunc::MD5:
      return EVP_md5();
    case CryptoHashFunc::SHA1:
      return EVP_sha1();
    case CryptoHashFunc::SHA224:
      return EVP_sha224();
    case CryptoHashFunc::SHA256:
      return EVP_sha256();
    case CryptoHashFunc::SHA384:
      return EVP_sha384();
    case CryptoHashFunc::SHA512:
      return EVP_sha512();
    default:
      throw;
  }
}

/**
 * Get cipher for a given algorithm and mode combination. Throws assertion
 * if desired combination does not exist.
 */
const EVP_CIPHER *getCipher(const CryptoAlgo algo,
                            const size_t nbBits,
                            const CryptoMode mode)
{
#define SF_CRYPTO_GET_CIPHER(algo, nbBits) \
  switch (mode) \
  { \
    case CryptoMode::CBC: return SF_CRYPTO_CIPHER(algo, nbBits, cbc)(); \
    case CryptoMode::CFB: return SF_CRYPTO_CIPHER(algo, nbBits, cfb)(); \
    case CryptoMode::ECB: return SF_CRYPTO_CIPHER(algo, nbBits, ecb)(); \
    case CryptoMode::GCM: return SF_CRYPTO_CIPHER(algo, nbBits, gcm)(); \
    case CryptoMode::OFB: return SF_CRYPTO_CIPHER(algo, nbBits, ofb)(); \
    default:              break; \
  } \
  break;

  switch (algo)
  {
    case CryptoAlgo::AES:
      switch (nbBits)
      {
        case 128:
        SF_CRYPTO_GET_CIPHER(aes, 128)
        case 192:
        SF_CRYPTO_GET_CIPHER(aes, 192)
        case 256:
        SF_CRYPTO_GET_CIPHER(aes, 256)
        default:
          break;
      }
      break;
    default:
      break;
  }

  // If we get here, we failed to get into one of the case-branches that
  // returns a cipher.
  /*SF_ASSERT_ALWAYS("invalid_cipher",
                   static_cast<int>(algo) % nbBits %
                   static_cast<int>(mode));*/

#undef SF_CRYPTO_GET_CIPHER
  return nullptr;
}

Cryptor::Cryptor()
{
  // Initialize static mutexes for use by OpenSSL.
  /*s_mutexes = new MutexVector(CRYPTO_num_locks());
  for (MutexSlot &slot : *s_mutexes)
    slot.second = false;

  // Set threading callbacks for OpenSSL.
  CRYPTO_set_locking_callback(lock);
  CRYPTO_set_id_callback(pthread_self);
  CRYPTO_THREADID_set_callback(getThreadId);
  CRYPTO_set_dynlock_create_callback(dynlockCreate);
  CRYPTO_set_dynlock_destroy_callback(dynlockDestroy);
  CRYPTO_set_dynlock_lock_callback(dynlockLock);*/

  // Initialize OpenSSL.
  ERR_load_crypto_strings();
  for (const CryptoAlgo algo : {CryptoAlgo::AES})
    for (const int nbBits : {128, 192, 256})
      for (const CryptoMode mode : {CryptoMode::CBC,
                                    CryptoMode::CFB,
                                    CryptoMode::ECB,
                                    CryptoMode::GCM,
                                    CryptoMode::OFB})
        EVP_add_cipher(getCipher(algo, nbBits, mode));
  OPENSSL_no_config();
}

Cryptor::~Cryptor()
{
  // Tear-down OpenSSL.
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  //ERR_remove_state(0);
  ERR_free_strings();

  // Release static mutexes.
  /*for (MutexSlot &slot : *s_mutexes)
    if (slot.second)
      _mutex_term(&slot.first);
  delete s_mutexes;
  s_mutexes = nullptr;*/
}

void Cryptor::generateKey(CryptoKey &key,
                          const size_t nbBits,
                          const CryptoRandomDevice dev)
{
  /*SF_ASSERT((nbBits == 128) || (nbBits == 192) || (nbBits == 256),
            "illegal_key_size",
            nbBits);*/
  key.nbBits = nbBits;
  memset(key.data, 0, sizeof(key.data));
  getRandomBytes(key.data, nbBits / 8, dev);
}

void Cryptor::generateIV(CryptoIV &iv,
                         CryptoRandomDevice dev)
{
  getRandomBytes(iv.data, sizeof(iv.data), dev);
}

CipherContext Cryptor::createCipherContext(const CryptoAlgo algo,
                                           const CryptoMode mode,
                                           const CryptoPadding padding,
                                           const CryptoKey &key,
                                           const CryptoIV &iv,
                                           const bool externalDecryptErrors)
{
  return this->createCipherContext(algo, mode, padding,
                                   ::std::make_shared<CryptoKey>(key),
                                   iv, externalDecryptErrors);
}

CipherContext Cryptor::createCipherContext(
  const CryptoAlgo algo,
  const CryptoMode mode,
  const CryptoPadding padding,
  const ::std::shared_ptr<const CryptoKey> &key,
  const CryptoIV &iv,
  const bool externalDecryptErrors)
{
  return CipherContext(algo, mode, padding, key, iv,
                       externalDecryptErrors);
}

HashContext Cryptor::createHashContext(const CryptoHashFunc func)
{
  return HashContext(func);
}

}
}
}
