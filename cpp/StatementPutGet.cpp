/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <iostream>
#include "StatementPutGet.hpp"

using namespace Snowflake::Client;

StatementPutGet::StatementPutGet(SF_STMT *stmt) :
  m_stmt(stmt)
{
}

bool StatementPutGet::parsePutGetCommand(std::string *sql,
                                         PutGetParseResponse *putGetParseResponse)
{
  snowflake_query(m_stmt, sql->c_str(), 0);
  putGetParseResponse->updateWith(m_stmt->put_get_response);
  return true;
}