/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <stdio.h>
#include <snowflake/client.h>
#include <example_setup.h>
#include "IStatementPutGet.hpp"
#include "FileTransferAgent.hpp"
#include "StatementPutGet.hpp"

int main() {
  /* init */
  SF_STATUS status;
  initialize_snowflake_example(SF_BOOLEAN_FALSE);
  SF_CONNECT *sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    fprintf(stderr, "Connecting to snowflake failed, exiting...\n");
    SF_ERROR *error = snowflake_error(sf);
    fprintf(stderr, "Error message: %s\nIn File, %s, Line, %d\n",
            error->msg, error->file, error->line);
    goto cleanup;
  }

  /* query */
  SF_STMT *sfstmt = snowflake_stmt(sf);

  std::string sql = "put file:///home/hyu/test1.csv @~";

  Snowflake::Client::IStatementPutGet *stmtPutGet = new
    Snowflake::Client::StatementPutGet(sfstmt);

  Snowflake::Client::FileTransferAgent agent(stmtPutGet);

  Snowflake::Client::FileTransferResult * result = agent.execute(&sql);

  cleanup:
  /* delete statement */
  snowflake_stmt_term(sfstmt);
  delete stmtPutGet;

  /* close and term */
  snowflake_term(sf); // purge snowflake context
  snowflake_global_term();

  return status;
}
