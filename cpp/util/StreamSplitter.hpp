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


private:
  const unsigned int m_capacity;

  char * m_dataBuffer;

  virtual int underflow();
};

class StreamSplitter
{
public:
  StreamSplitter(std::basic_iostream<char> * inputStream,
                 unsigned int numOfParts,
                 unsigned int partMaxSize);

  ~StreamSplitter();

  void addSplitPartToQueue();

  std::basic_iostream<char>* getNextSplitPart();

  void popFrontSplitPart();

  unsigned int getTotalParts(long long int streamSize);

private:
  std::vector<ByteArrayStreamBuf *> streamBuffers;

  SF_MUTEX_HANDLE streamMutex;
};

}
}
}

#endif //SNOWFLAKECLIENT_STREAMSPLITTER_HPP
