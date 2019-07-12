/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_HASHCONTEXT_HPP
#define SNOWFLAKECLIENT_HASHCONTEXT_HPP

#include "CryptoTypes.hpp"
#include <memory>

namespace Snowflake
{
namespace Client
{
namespace Crypto
{


/**
 * Cryptographic hash context.
 *
 * Context objects behave like std::unique_ptrs. They cannot be copied, but
 * they can be moved. On destruction, a context object will release any
 * underlying library resources it owns.
 */
class HashContext final
{
public:

  /**
   * Default constructor.
   *
   * Context objects in the default-constructed state cannot be used. They
   * need to be move-assigned from another valid context object first.
   */
  HashContext();

  /**
   * Destructor.
   *
   * Frees any underlying library resources. It is okay to have an unfinished
   * operation (but it will be impossible to continue with that operation).
   */
  ~HashContext();

  /**
   * Move constructor.
   *
   * Leaves the other context in an undefined but destructible state.
   */
  HashContext(HashContext &&other) noexcept;

  /**
   * Move-assignment operator.
   *
   * Leaves the other context in an undefined but destructible state.
   */
  HashContext &operator=(HashContext &&other) noexcept;

  /**
   * Reset into default-constructed state. Frees any underlying library
   * resources.
   */
  void reset() noexcept;

  /**
   * Whether or not this is a valid context that can be used for hashing.
   * Returns 'false' for default constructed context object or context objects
   * that have been the source of a move construction or move assignment.
   */
  inline bool isValid() const noexcept;

  /**
   * Whether or not a hash operation is in progress; that is, initialize() has
   * been called and there has not been a matching call to finalize().
   */
  bool isActive() const noexcept;

  /**
   * Swap state of two contexts.
   */
  inline void swap(HashContext &other) noexcept;

  /**
   * Get message digest size in number of bytes.
   */
  size_t getDigestSize() noexcept;

  /**
   * Initialize a new hash operation. Must be called before any call to next()
   * or finalize().
   */
  void initialize();

  /**
   * Ingest another block of data. May be called multiple times between a
   * matching pair of calls to initialize() and finalize().
   *
   * @param data
   *    Input block.
   * @param len
   *    Size of input block in number of bytes.
   */
  void next(const void *data,
            size_t len);

  /**
   * Finalize current operation. After finalize() has been called, another
   * operation can be started by calling initialize().
   *
   * @param(OUT) digest
   *    Output message digest. Must be of sufficient size, the size depending
   *    on the hash function.
   */
  void finalize(void *digest);

private:

  friend class Cryptor;

  /**
   * Hash context implementation class.
   */
  class Impl;

  /**
   * Constructor for creating a valid context. Can only be called by the
   * Crypto facade. Ensures that the crypto library is initialized before
   * context objects are used.
   *
   * @param func
   *    Cryptographic hash function of choice. Determines the concrete
   *    implementation class.
   */
  explicit HashContext(CryptoHashFunc func);

  /// Pointer to implementation ("pimpl").
  std::unique_ptr<Impl> m_pimpl;

};

inline bool HashContext::isValid() const noexcept
{
  return !!m_pimpl;
}

inline void HashContext::swap(HashContext &other) noexcept
{
  m_pimpl.swap(other.m_pimpl);
}

} // namespace sf
}
}


#endif //SNOWFLAKECLIENT_HASHCONTEXT_HPP
