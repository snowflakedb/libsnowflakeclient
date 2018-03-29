/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "CipherStream.hpp"
#include "Cryptor.hpp"
#include <iostream>
#include <algorithm>

namespace Snowflake
{
namespace Client
{
namespace Crypto
{

CipherBuf::CipherBuf(::std::basic_streambuf<char> *streambuf,
                     CryptoOperation op,
                     CryptoKey &key,
                     CryptoIV &iv,
                     size_t block_size) :
  m_streambuf(streambuf),
  m_blockSize(block_size),
  m_srcBuffer(new char[block_size]),
  m_readReachEnds(false)
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
}

CipherBuf::~CipherBuf()
{
  delete[] m_srcBuffer;
  delete[] m_resultBuffer;
}

int CipherBuf::underflow()
{
  if (m_readReachEnds)
  {
    return ::std::char_traits<char>::eof();
  }

  // read pointer has reached end, need to refill data into buffer
  if (this->gptr() == this->egptr())
  {
    size_t nextSize;
    do
    {
      ::std::streamsize bytesRead = m_streambuf->sgetn(m_srcBuffer,
                                                       m_blockSize);
      m_readReachEnds = bytesRead < m_blockSize;
      nextSize = m_cipherCtx.next(m_resultBuffer, m_srcBuffer,
                                  (size_t) bytesRead);
    } while (nextSize == 0 && !m_readReachEnds);

    size_t finalizeSize = nextSize != 0 ? 0 :
                          m_cipherCtx.finalize(m_resultBuffer + nextSize);

    this->setg(m_resultBuffer, m_resultBuffer,
               m_resultBuffer + nextSize + finalizeSize);
  }
  return ::std::char_traits<char>::to_int_type(*this->gptr());
}

}
}
}