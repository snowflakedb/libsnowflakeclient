/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_ITRANSFERRESULT_HPP
#define SNOWFLAKECLIENT_ITRANSFERRESULT_HPP

namespace Snowflake
{
namespace Client
{

/**
 * Interface consumed by external component to get transfer result
 */
class ITransferResult
{
public:
  virtual ~ITransferResult() {}

  /**
   * @return return if has more result otherwise false
   */
  virtual bool next() = 0;

  /**
   * @return file transfer result
   */
  virtual const char * getStatus() = 0;

  /**
   * @return result size, a.k.a number of file that has been transferred
   */
  virtual int getResultSize() = 0;
};
}
}

#endif //SNOWFLAKECLIENT_ITRANSFERRESULT_HPP
