/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/logger.h"
#include "ByteArrayStreamBuf.hpp"
#include <cstring>

Snowflake::Client::Util::ByteArrayStreamBuf::ByteArrayStreamBuf(
  unsigned int capacity) :
  m_capacity(capacity)
{
  m_dataBuffer = new char[capacity];
  memset(m_dataBuffer, 0, capacity);
  setg(m_dataBuffer, m_dataBuffer, m_dataBuffer+capacity);
  setp(m_dataBuffer, m_dataBuffer + capacity);
}

Snowflake::Client::Util::ByteArrayStreamBuf::~ByteArrayStreamBuf()
{
  delete[] m_dataBuffer;
}

void * Snowflake::Client::Util::ByteArrayStreamBuf::updateSize(
  long updatedSize)
{
  this->setg(m_dataBuffer, m_dataBuffer, m_dataBuffer + updatedSize);
  this->setp(m_dataBuffer, m_dataBuffer + updatedSize);
  this->size = updatedSize;
  return nullptr;
}

Snowflake::Client::Util::StreamSplitter::StreamSplitter(
  std::basic_iostream<char> *inputStream,
  unsigned int numOfBuffer,
  unsigned int partMaxSize) :
  m_inputStream(inputStream),
  m_partMaxSize(partMaxSize),
  m_currentPartIndex(-1)
{
  _critical_section_init(&streamMutex);
  for (unsigned int i=0; i<numOfBuffer; i++)
  {
    buffers.push_back(new ByteArrayStreamBuf(partMaxSize));
  }
}

Snowflake::Client::Util::ByteArrayStreamBuf*
Snowflake::Client::Util::StreamSplitter::FillAndGetBuf(
  int bufIndex, int &partIndex)
{
  _critical_section_lock(&streamMutex);
  ByteArrayStreamBuf * buf = buffers[bufIndex];
  memset(buf->getDataBuffer(), 0, m_partMaxSize);
  m_inputStream->read(buf->getDataBuffer(), m_partMaxSize);
  buf->updateSize((long)m_inputStream->gcount());
  m_currentPartIndex ++;
  partIndex = m_currentPartIndex;
  _critical_section_unlock(&streamMutex);
  return buf;
}

Snowflake::Client::Util::StreamSplitter::~StreamSplitter()
{
  _critical_section_term(&streamMutex);
  for (unsigned int i=0; i<buffers.size(); i++)
  {
    delete buffers[i];
  }
}

unsigned int Snowflake::Client::Util::StreamSplitter::getTotalParts(
  long long int streamSize)
{
  return (unsigned int)(streamSize/m_partMaxSize + 1);
}

Snowflake::Client::Util::StreamAppender::StreamAppender(
  std::basic_iostream<char> *outputStream, int totalPartNum, int parallel,
  int partSize)
  : m_outputStream(outputStream),
    m_totalPartNum(totalPartNum),
    m_parallel(parallel),
    m_partSize(partSize),
    m_currentPartIndex(0)
{
  m_buffers = new ByteArrayStreamBuf*[parallel];
  for (int i = 0; i < m_parallel; i++)
  {
    m_buffers[i] = nullptr;
  }

  _critical_section_init(&m_streamMutex);
  _cond_init(&m_streamCv);
}

Snowflake::Client::Util::StreamAppender::~StreamAppender()
{
  for (int i = 0; i < m_parallel; i++)
  {
    if (m_buffers[i] != nullptr)
    {
      delete m_buffers[i];
    }
  }

  delete[] m_buffers;
}

Snowflake::Client::Util::ByteArrayStreamBuf * Snowflake::Client::Util::
StreamAppender::GetBuffer(
  int threadId)
{
  if (m_buffers[threadId] == nullptr)
  {
    m_buffers[threadId] = new ByteArrayStreamBuf(m_partSize);
  }
  return m_buffers[threadId];
}

void Snowflake::Client::Util::StreamAppender::WritePartToOutputStream(
  int threadId, int partIndex)
{
  _critical_section_lock(&m_streamMutex);
  while(partIndex > m_currentPartIndex)
  {
    _cond_wait(&m_streamCv, &m_streamMutex);
  }
  m_outputStream->write(m_buffers[threadId]->getDataBuffer(),
                        m_buffers[threadId]->getSize());

  m_currentPartIndex ++;

  if (partIndex == m_totalPartNum - 1)
  {
    m_outputStream->flush();
  }
  _cond_broadcast(&m_streamCv);
  _critical_section_unlock(&m_streamMutex);
}
