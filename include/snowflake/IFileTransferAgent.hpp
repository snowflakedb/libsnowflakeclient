/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_IFILETRANSFERAGENT_HPP
#define SNOWFLAKECLIENT_IFILETRANSFERAGENT_HPP

#include "ITransferResult.hpp"
#include "IStatementPutGet.hpp"

namespace Snowflake
{
namespace Client
{

class IFileTransferAgent
{
public:
  virtual ~IFileTransferAgent() {};
  /**
   * Called by external component to execute put/get command
   * @param command put/get command
   * @return a fixed view result set representing upload/download result
   */
  virtual ITransferResult *execute(std::string *command) = 0;

  /**
   * Static method to instantiate a IFileTransferAgent class
   */
  static IFileTransferAgent *getTransferAgent(
    IStatementPutGet * statementPutGet);
};

}
}
#endif //SNOWFLAKECLIENT_IFILETRANSFERAGENT_HPP
