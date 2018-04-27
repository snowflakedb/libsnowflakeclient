/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_STREAMSPLITTER_HPP
#define SNOWFLAKECLIENT_STREAMSPLITTER_HPP

#include <streambuf>
#include <istream>
#include <deque>
#include <vector>
#include <snowflake/platform.h>

namespace Snowflake
{
namespace Client
{
namespace Util
{

/**
 * Simple in memory byte array buffer
 */
class ByteArrayStreamBuf : public std::basic_streambuf<char>
{
public:
  ByteArrayStreamBuf(unsigned int capacity);

  ~ByteArrayStreamBuf();

  inline const unsigned int getCapacity()
  {
    return m_capacity;
  }

  inline char * getDataBuffer()
  {
    return m_dataBuffer;
  }

  void * updateSize(long updatedSize);

  inline long getSize()
  {
    return size;
  }


private:
  const unsigned int m_capacity;

  long size;

  char * m_dataBuffer;
};

/**
 * Split stream into parts. Manage in memory buffer to be reused.
 */
class StreamSplitter
{
public:
  StreamSplitter(std::basic_iostream<char> * inputStream,
                 unsigned int numOfBuffer,
                 unsigned int partMaxSize);

  ~StreamSplitter();

  ByteArrayStreamBuf * FillAndGetBuf(int bufIndex, int &partIndex);

  unsigned int getTotalParts(long long int streamSize);

private:
  SF_MUTEX_HANDLE streamMutex;

  /// input stream to be splitted.
  std::basic_iostream<char> * m_inputStream;

  unsigned int m_partMaxSize;

  int m_currentPartIndex;

  std::vector<ByteArrayStreamBuf *> buffers;
};

}
}
}

#endif //SNOWFLAKECLIENT_STREAMSPLITTER_HPP
