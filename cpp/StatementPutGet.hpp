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

/**
 * C API implementation of IStatementPutGet.
 * Communicate with server to parse statement
 */
class StatementPutGet : public Snowflake::Client::IStatementPutGet
{
public:
  StatementPutGet(SF_STMT *stmt);

  virtual bool parsePutGetCommand(std::string *sql,
                                  PutGetParseResponse *putGetParseResponse);

private:
  SF_STMT *m_stmt;
};
}
}


#endif //SNOWFLAKECLIENT_STATEMENTPUTGET_HPP
