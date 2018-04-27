/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "StreamSplitter.hpp"
#include <cstring>

Snowflake::Client::Util::ByteArrayStreamBuf::ByteArrayStreamBuf(
  unsigned int capacity) :
  m_capacity(capacity)
{
  m_dataBuffer = new char[capacity];
  memset(m_dataBuffer, 0, capacity);
  setg(m_dataBuffer, m_dataBuffer, m_dataBuffer+capacity);
}

Snowflake::Client::Util::ByteArrayStreamBuf::~ByteArrayStreamBuf()
{
  delete[] m_dataBuffer;
}

void * Snowflake::Client::Util::ByteArrayStreamBuf::updateSize(
  long updatedSize)
{
  this->setg(m_dataBuffer, m_dataBuffer, m_dataBuffer + updatedSize);
  this->size = updatedSize;
}

Snowflake::Client::Util::StreamSplitter::StreamSplitter(
  std::basic_iostream<char> *inputStream,
  unsigned int numOfBuffer,
  unsigned int partMaxSize) :
  m_inputStream(inputStream),
  m_partMaxSize(partMaxSize),
  m_currentPartIndex(-1)
{
  _mutex_init(&streamMutex);
  for (int i=0; i<numOfBuffer; i++)
  {
    buffers.push_back(new ByteArrayStreamBuf(partMaxSize));
  }
}

Snowflake::Client::Util::ByteArrayStreamBuf*
Snowflake::Client::Util::StreamSplitter::FillAndGetBuf(
  int bufIndex, int &partIndex)
{
  _mutex_lock(&streamMutex);
  ByteArrayStreamBuf * buf = buffers[bufIndex];
  memset(buf->getDataBuffer(), 0, m_partMaxSize);
  m_inputStream->read(buf->getDataBuffer(), m_partMaxSize);
  buf->updateSize(m_inputStream->gcount());
  m_currentPartIndex ++;
  partIndex = m_currentPartIndex;
  _mutex_unlock(&streamMutex);
  return buf;
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
