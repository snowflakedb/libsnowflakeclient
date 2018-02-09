//
// Created by hyu on 2/2/18.
//

#ifndef SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
#define SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP

#include "IStatementPutGet.hpp"
#include "FileTransferResult.hpp"
#include "string"

namespace Snowflake
{
  namespace Client
  {
    /**
     * This is the main class to external component (c api or ODBC)
     * External component should implement IStatement interface to submit put
     * or get command to server to do parsing.
     */
    class FileTransferAgent
    {
    public:
      FileTransferAgent(IStatementPutGet * statement);

      /**
       * Called by external component to execute put/get command
       * @param command put/get command
       * @return a fixed view result set representing upload/download result
       */
      FileTransferResult* execute(std::string * command);

    private:

      void upload();

      void download();

      IStatementPutGet * m_stmtPutGet;
    };
  }
}

#endif //SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
