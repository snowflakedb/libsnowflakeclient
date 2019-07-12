/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
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

  /**
   * Read data from input stream into in memory buffer
   */
  ByteArrayStreamBuf * FillAndGetBuf(int bufIndex, int &partIndex);

  /// return total parts given total input stream size
  unsigned int getTotalParts(long long int streamSize);

private:
  /// mutex for threads to read data
  SF_CRITICAL_SECTION_HANDLE streamMutex;

  /// input stream to be splitted.
  std::basic_iostream<char> * m_inputStream;

  /// part size
  unsigned int m_partMaxSize;

  /// current parts have been read
  int m_currentPartIndex;

  /// array of in memory buffer
  std::vector<ByteArrayStreamBuf *> buffers;
};

/**
 * Append part stream into single final output file stream
 */
class StreamAppender
{
public:
  StreamAppender(std::basic_iostream<char> * outputStream,
                 int totalPartNum, int parallel, int partSize);

  ~StreamAppender();

  /**
   * Write single part into output stream. Will wait for other thread to
   * write first so that output file is in order
   */
  void WritePartToOutputStream(int threadId, int partIndex);

  /**
   * Get in memory buffer to store the data downloaded before writing to
   * output stream
   */
  ByteArrayStreamBuf * GetBuffer(int threadId);

private:

  /// output stream to append to
  std::basic_iostream<char> * m_outputStream;

  /// mutex for multiple thread write into one outputstream
  SF_CRITICAL_SECTION_HANDLE m_streamMutex;

  /// appended stream cv
  SF_CONDITION_HANDLE m_streamCv;

  /// number of buffer
  int m_parallel;

  /// total part number
  int m_totalPartNum;

  /// part size
  int m_partSize;

  /// arrays of memory buffer
  ByteArrayStreamBuf** m_buffers;

  /// index of part that will be written to target ouput stream
  int m_currentPartIndex;
};

}
}
}

#endif //SNOWFLAKECLIENT_STREAMSPLITTER_HPP
