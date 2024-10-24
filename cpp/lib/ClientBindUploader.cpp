/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */


#include <sstream>

#include "ClientBindUploader.hpp"
#include "../logger/SFLogger.hpp"
#include "snowflake/basic_types.h"
#include "snowflake/SF_CRTFunctionSafe.h"
#include "../util/SnowflakeCommon.hpp"

namespace Snowflake
{
namespace Client
{
ClientBindUploader::ClientBindUploader(SF_STMT *sfstmt,
                                       const std::string& stageDir,
                                       unsigned int numParams, unsigned int numParamSets,
                                       unsigned int maxFileSize,
                                       int compressLevel) :
  BindUploader(stageDir, numParams, numParamSets, maxFileSize, compressLevel),
  m_stmt(sfstmt)
{
  ; // Do nothing
}

void ClientBindUploader::createStageIfNeeded()
{
  // Check the flag without locking to get better performance.
  if (m_connection.isArrayBindStageCreated())
  {
    return;
  }

  MutexGuard guard(m_connection.getArrayBindingMutex());

  // another thread may have created the session by the time we enter this block
  // so check the flag again.
  if (m_connection.isArrayBindStageCreated())
  {
    return;
  }

  try
  {
    sf::Statement statement(m_connection);
    statement.executeQuery(CREATE_STAGE_STMT.GetAsUTF8(), false, true);
    m_connection.setArrayBindStageCreated();
  }
  catch (...)
  {
    SF_TRACE_LOG("sf", "BindUploader", "createStageIfNeeded",
      "Failed to create temporary stage for array binds.%s", "");
    throw;
  }
}

void ClientBindUploader::executeUploading(const std::string &sql,
                                          std::basic_iostream<char>& uploadStream,
                                          size_t dataSize)
{
}

} // namespace Client
} // namespace Snowflake
