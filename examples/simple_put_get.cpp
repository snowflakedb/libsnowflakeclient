/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <stdio.h>
#include <snowflake/client.h>
#include <example_setup.h>
#include "IStatementPutGet.hpp"
#include "FileTransferAgent.hpp"
#include "StatementPutGet.hpp"
#include <iostream>

int main() {
  /* init */
  SF_STATUS status;
  initialize_snowflake_example(SF_BOOLEAN_FALSE);
  SF_CONNECT *sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    fprintf(stderr, "Connecting to snowflake failed, exiting...\n");
    SF_ERROR_STRUCT *error = snowflake_error(sf);
    fprintf(stderr, "Error message: %s\nIn File, %s, Line, %d\n",
            error->msg, error->file, error->line);

    snowflake_term(sf); // purge snowflake context
    snowflake_global_term();
    exit (1);
  }
  SF_STMT *sfstmt = NULL;
  Snowflake::Client::IStatementPutGet *stmtPutGet = NULL;
  try
  {
    /* query */
    sfstmt = snowflake_stmt(sf);
    std::string sql("put file:///tmp/test1.csv @~");

    stmtPutGet = new Snowflake::Client::StatementPutGet(sfstmt);

    Snowflake::Client::FileTransferAgent agent(stmtPutGet);

    agent.execute(&sql);

    status = SF_STATUS_SUCCESS;
  }
  catch (...)
  {
    status = SF_STATUS_ERROR_GENERAL;
  }

  // cleanup
  if (stmtPutGet)
  {
    delete stmtPutGet;
  }
  snowflake_stmt_term(sfstmt);

  /* close and term */
  snowflake_term(sf); // purge snowflake context
  snowflake_global_term();

  return status;
}
