/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/logger.h"
#include "CipherStreamBuf.hpp"
#include "Cryptor.hpp"
#include <iostream>
#include <algorithm>

namespace Snowflake
{
namespace Client
{
namespace Crypto
{

CipherStreamBuf::CipherStreamBuf(::std::basic_streambuf<char> *streambuf,
                                 CryptoOperation op,
                                 CryptoKey &key,
                                 CryptoIV &iv,
                                 size_t block_size) :
  m_streambuf(streambuf),
  m_blockSize(block_size),
  m_srcBuffer(new char[block_size]),
  m_readReachEnds(false),
  m_finalized(false)
{
  // initialize cipher context
  m_cipherCtx = Cryptor::getInstance().createCipherContext(
    CryptoAlgo::AES,
    CryptoMode::CBC, CryptoPadding::PKCS5, key, iv, false);
  m_cipherCtx.initialize(op);

  // result size should be batch size plus aes block size
  // because padding might be required
  size_t resultLen = block_size + cryptoAlgoBlockSize(CryptoAlgo::AES);
  m_resultBuffer = new char[resultLen];
  memset(m_srcBuffer, 0, m_blockSize);

  char *end = m_resultBuffer + resultLen;
  this->setg(m_resultBuffer, end, end);
  this->setp(m_srcBuffer, m_srcBuffer + block_size);
}

CipherStreamBuf::~CipherStreamBuf()
{
  delete[] m_srcBuffer;
  delete[] m_resultBuffer;
}

int CipherStreamBuf::underflow()
{
  while(this->gptr() == this->egptr() && !m_readReachEnds)
  {
    ::std::streamsize bytesRead = m_streambuf->sgetn(m_srcBuffer,
                                                     m_blockSize);

    m_readReachEnds = (size_t)bytesRead < m_blockSize;
    size_t nextSize = m_cipherCtx.next(m_resultBuffer, m_srcBuffer,
                                (size_t) bytesRead);


    this->setg(m_resultBuffer, m_resultBuffer,
               m_resultBuffer + nextSize);
  }

  if (this->gptr() == this->egptr() && m_readReachEnds && !m_finalized)
  {
    size_t finalizeSize = m_cipherCtx.finalize(m_resultBuffer);
    this->setg(m_resultBuffer, m_resultBuffer, m_resultBuffer + finalizeSize);
    m_finalized = true;
  }

  return this->gptr() == this->egptr() ? ::std::char_traits<char>::eof() :
    ::std::char_traits<char>::to_int_type(*this->gptr());

}

int CipherStreamBuf::overflow(int_type ch)
{
  if (pptr() - pbase() > 0)
  {
    size_t nextSize = m_cipherCtx.next(m_resultBuffer, m_srcBuffer,
                                       pptr() - pbase());

    size_t finalSize = traits_type::eq_int_type(ch, traits_type::eof()) ?
      m_cipherCtx.finalize(m_resultBuffer + nextSize) : 0;

    std::streamsize written = m_streambuf->sputn(m_resultBuffer,
                                                 nextSize + finalSize);
    if ((size_t)written < nextSize)
    {
      return traits_type::eof();
    }

    setp(m_srcBuffer, m_srcBuffer + m_blockSize);

    if (!traits_type::eq_int_type(ch, traits_type::eof()))
      sputc(ch);

    return traits_type::not_eof(ch);
  }
  else
  {
    return traits_type::eof();
  }
}

int CipherStreamBuf::sync()
{
  std::basic_streambuf<char>::int_type result = this->overflow(traits_type::eof());
  m_streambuf->pubsync();
  return traits_type::eq_int_type(result, traits_type::eof()) ? -1 : 0;
}

}
}
}