//
// Created by hyu on 2/7/18.
//

#ifndef SNOWFLAKECLIENT_STATEMENTPUTGET_HPP
#define SNOWFLAKECLIENT_STATEMENTPUTGET_HPP

#include "IStatementPutGet.hpp"
#include "PutGetParseResponse.hpp"
#include "snowflake/client.h"

namespace Snowflake
{
  namespace Client
  {
    class StatementPutGet : public Snowflake::Client::IStatementPutGet
    {
    public:
      StatementPutGet(SF_STMT *stmt);

      virtual PutGetParseResponse* parsePutGetCommand(std::string *sql);

    private:
      SF_STMT * m_stmt;
    };
  }
}


#endif //SNOWFLAKECLIENT_STATEMENTPUTGET_HPP
