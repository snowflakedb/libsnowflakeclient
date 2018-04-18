/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_CIPHERSTREAM_HPP
#define SNOWFLAKECLIENT_CIPHERSTREAM_HPP

#include <streambuf>
#include <istream>
#include "CipherContext.hpp"

namespace Snowflake
{
namespace Client
{
namespace Crypto
{

/**
 * Underlying stream buf object for a CipherStream class
 */
class CipherStreamBuf : public std::basic_streambuf<char>
{
public:
  CipherStreamBuf(std::basic_streambuf<char> *streambuf,
            CryptoOperation op,
            CryptoKey &key,
            CryptoIV &iv,
            size_t block_size);

  ~CipherStreamBuf();

private:
  /// underlying stream buf
  std::basic_streambuf<char> *m_streambuf;

  /// array to store source data
  char *m_srcBuffer;

  /// array to store encrypted/decrypted block data
  char *m_resultBuffer;

  /// buffer size for each batch to be encrypted/decrypted
  /// note this is different from encryption block size
  size_t m_blockSize;

  /// cipher context
  CipherContext m_cipherCtx;

  /// true if reads from src streambuf has reached the end
  bool m_readReachEnds;

  bool m_finalized;

  virtual int underflow();

};

}
}
}


#endif //SNOWFLAKECLIENT_CIPHERSTREAM_HPP
