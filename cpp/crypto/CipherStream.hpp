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
class CipherBuf : public std::basic_streambuf<char>
{
public:
  CipherBuf(std::basic_streambuf<char> *streambuf,
            CryptoOperation op,
            CryptoKey &key,
            CryptoIV &iv,
            size_t block_size);

  ~CipherBuf();

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

  virtual int underflow();

};


class CipherStream : public std::basic_iostream<char>
{
public:
  CipherStream(std::basic_iostream<char> &stream,
               CryptoOperation op,
               CryptoKey &key,
               CryptoIV &iv) :
    std::basic_iostream<char>(
      new CipherBuf(stream.rdbuf(), op, key, iv, 16)) {}

  virtual ~CipherStream()
  {
    delete rdbuf();
  }

};
}
}
}


#endif //SNOWFLAKECLIENT_CIPHERSTREAM_HPP
