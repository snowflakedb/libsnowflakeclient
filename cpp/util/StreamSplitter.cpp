/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "StreamSplitter.hpp"

Snowflake::Client::Util::ByteArrayStreamBuf::ByteArrayStreamBuf(
  unsigned int capacity) :
  m_capacity(capacity)
{
  m_dataBuffer = new char[capacity];
  setg(m_dataBuffer, m_dataBuffer, m_dataBuffer+capacity);
}

Snowflake::Client::Util::ByteArrayStreamBuf::~ByteArrayStreamBuf()
{
  delete[] m_dataBuffer;
}

int Snowflake::Client::Util::ByteArrayStreamBuf::underflow()
{
  return ::std::char_traits<char>::to_int_type(*this->gptr());
}


