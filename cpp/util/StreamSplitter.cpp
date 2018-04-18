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
  m_streamBuffers(numOfBuffer, nullptr),
  m_bufferInUse(numOfBuffer, false),
  m_inputStream(inputStream),
  m_partMaxSize(partMaxSize)
{
  _mutex_init(&streamMutex);
}

Snowflake::Client::Util::ByteArrayStreamBuf *
Snowflake::Client::Util::StreamSplitter::getNextSplitPart()
{
  _mutex_lock(&streamMutex);
  int idx = checkBufferNotInUseIdx();
  m_bufferInUse[idx] = true;

  if (m_streamBuffers[idx] == nullptr)
  {
    m_streamBuffers[idx] = new ByteArrayStreamBuf(m_partMaxSize);
  }

  ByteArrayStreamBuf * returnVal = nullptr;
  m_inputStream->read(
    m_streamBuffers[idx]->getDataBuffer(), m_partMaxSize);
  m_streamBuffers[idx]->updateSize(m_inputStream->gcount());
  returnVal = m_streamBuffers[idx];
  _mutex_unlock(&streamMutex);

  return returnVal;
}

Snowflake::Client::Util::StreamSplitter::~StreamSplitter()
{
  for (auto it = m_streamBuffers.begin(); it != m_streamBuffers.end(); it++)
  {
    if ((*it) != nullptr)
    {
      delete *it;
    }
  }

  _mutex_term(&streamMutex);
}

int Snowflake::Client::Util::StreamSplitter::checkBufferNotInUseIdx()
{
  for (int i=0; i<m_bufferInUse.size(); i++)
  {
    if (!m_bufferInUse.at(i))
    {
      return i;
    }
  }
}

unsigned int Snowflake::Client::Util::StreamSplitter::getTotalParts(
  long long int streamSize)
{
  return streamSize/m_partMaxSize + 1;
}

void Snowflake::Client::Util::StreamSplitter::markDone(
  Snowflake::Client::Util::ByteArrayStreamBuf * buf)
{
  _mutex_lock(&streamMutex);
  for (size_t i=0; i<m_streamBuffers.size(); i++)
  {
    if (m_streamBuffers.at(i) == buf)
    {
      m_bufferInUse[i] = false;
    }
  }
  _mutex_unlock(&streamMutex);
}

