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
  return this->gptr() == this->egptr() ? std::char_traits<char>::eof() :
    ::std::char_traits<char>::to_int_type(*this->gptr());
}

Snowflake::Client::Util::StreamSplitter::StreamSplitter(
  std::basic_iostream<char> *inputStream,
  unsigned int numOfBuffer,
  unsigned int partMaxSize) :
  m_inputStream(inputStream),
  m_partMaxSize(partMaxSize)
{
  _mutex_init(&streamMutex);
  for (int i=0; i<numOfBuffer; i++)
  {
    buffers.push_back(new ByteArrayStreamBuf(partMaxSize));
  }
}

Snowflake::Client::Util::ByteArrayStreamBuf*
Snowflake::Client::Util::StreamSplitter::FillAndGetBuf(int bufIndex)
{
  _mutex_lock(&streamMutex);
  m_inputStream->read(buffers[bufIndex]->getDataBuffer(), m_partMaxSize);
  buffers[bufIndex]->updateSize(m_inputStream->gcount());
  _mutex_unlock(&streamMutex);
  return buffers[bufIndex];
}

Snowflake::Client::Util::StreamSplitter::~StreamSplitter()
{
  _mutex_term(&streamMutex);
  for (unsigned int i=0; i<buffers.size(); i++)
  {
    delete buffers[i];
  }
}

unsigned int Snowflake::Client::Util::StreamSplitter::getTotalParts(
  long long int streamSize)
{
  return streamSize/m_partMaxSize + 1;
}
