/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "HashContext.hpp"
#include <openssl/evp.h>

namespace Snowflake
{
namespace Client
{
namespace Crypto
{

/**
 * Get digest routing for a given cryptographic hash function.
 *
 * Defined in Crypto.cpp
 */
extern const EVP_MD *getDigest(CryptoHashFunc func);

/**
 * Hash context implementation class.
 */
class HashContext::Impl final
{
public:
  explicit Impl(CryptoHashFunc func);

  ~Impl() noexcept;

  inline size_t getDigestSize() const noexcept { return m_digestSize; }

  inline bool isActive() const noexcept { return m_isActive; }

  inline void initialize();

  inline void next(const void *data,
                   size_t len);

  inline void finalize(void *digest);

private:
  bool m_isActive;
  const size_t m_digestSize;
  const EVP_MD *const m_md;
  EVP_MD_CTX *m_ctx;
};

HashContext::Impl::Impl(const CryptoHashFunc func)
  : m_isActive(false),
    m_digestSize(cryptoHashDigestSize(func)),
    m_md(getDigest(func))
{
  m_ctx = EVP_MD_CTX_new();
}

HashContext::Impl::~Impl() noexcept
{
  EVP_MD_CTX_free(m_ctx);
}

inline void HashContext::Impl::initialize()
{
  //SF_ASSERT0(!m_isActive, "already_initialized");
  if (!EVP_DigestInit_ex(m_ctx, m_md, nullptr))
    //SF_THROW_CRYPTO(&m_ctx % m_md);
    m_isActive = true;
}

inline void HashContext::Impl::next(const void *const data,
                                    const size_t len)
{
  //SF_ASSERT0(m_isActive, "not_initialized");
  if (!EVP_DigestUpdate(m_ctx, data, len))
    //SF_THROW_CRYPTO(&m_ctx % m_md % data % len);
    throw;
}

inline void HashContext::Impl::finalize(void *const digest)
{
  //SF_ASSERT0(m_isActive, "not_initialized");
  if (!EVP_DigestFinal_ex(m_ctx,
                          static_cast<unsigned char *>(digest), nullptr))
    //SF_THROW_CRYPTO(&m_ctx % m_md % digest);
    throw;
  m_isActive = false;
}

HashContext::HashContext(const CryptoHashFunc func)
  : m_pimpl(new Impl(func))
{
}

HashContext::HashContext() = default;

HashContext::~HashContext() = default;

HashContext::HashContext(HashContext &&other) noexcept = default;

HashContext &
HashContext::operator=(HashContext &&other) noexcept = default;

void HashContext::reset() noexcept
{
  m_pimpl.reset();
}

bool HashContext::isActive() const noexcept
{
  return m_pimpl && m_pimpl->isActive();
}

size_t HashContext::getDigestSize() noexcept
{
  //SF_ASSERT0(m_pimpl, "context_not_valid_1");
  return m_pimpl->getDigestSize();
}

void HashContext::initialize()
{
  //SF_ASSERT0(m_pimpl, "context_not_valid_2");
  m_pimpl->initialize();
}

void HashContext::next(const void *const data,
                       const size_t len)
{
  //SF_ASSERT0(m_pimpl, "context_not_valid_3");
  m_pimpl->next(data, len);
}

void HashContext::finalize(void *const digest)
{
  //SF_ASSERT0(m_pimpl, "context_not_valid_4");
  m_pimpl->finalize(digest);
}
}
}
}
