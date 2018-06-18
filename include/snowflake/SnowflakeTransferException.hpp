/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKETRANSFEREXCEPTION_HPP
#define SNOWFLAKECLIENT_SNOWFLAKETRANSFEREXCEPTION_HPP

#include <exception>

namespace Snowflake
{
namespace Client
{

enum TransferError
{
  INTERNAL_ERROR
};

class SnowflakeTransferException : public std::exception
{
public:
  SnowflakeTransferException(TransferError transferError, ...);

  const char * getErrorMessage();

  int getCode();

private:
  int m_code;

  const char * m_msg;
};
}
}




#endif //SNOWFLAKECLIENT_SNOWFLAKETRANSFEREXCEPTION_HPP
