//
// Created by hyu on 2/7/18.
//

#include "StatementPutGet.hpp"

using namespace Snowflake::Client;

StatementPutGet::StatementPutGet(SF_STMT *stmt) :
  m_stmt(stmt)
{
}

PutGetParseResponse* StatementPutGet::parsePutGetCommand(std::string *sql)
{
  snowflake_prepare(m_stmt, sql->c_str(), 0);
  snowflake_execute(m_stmt, SF_BOOLEAN_TRUE);
  return new PutGetParseResponse(m_stmt->put_get_response);
}