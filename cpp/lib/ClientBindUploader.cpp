/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */


#include <sstream>
#include <vector>

#include "ClientBindUploader.hpp"
#include "../logger/SFLogger.hpp"
#include "snowflake/basic_types.h"
#include "snowflake/SF_CRTFunctionSafe.h"
#include "../util/SnowflakeCommon.hpp"
#include "snowflake/Exceptions.hpp"
#include "client_int.h"
#include "results.h"
#include "error.h"

namespace Snowflake
{
namespace Client
{
ClientBindUploader::ClientBindUploader(SF_STMT *sfstmt,
                                       const std::string& stageDir,
                                       unsigned int numParams, unsigned int numParamSets,
                                       unsigned int maxFileSize,
                                       int compressLevel) :
  BindUploader(stageDir, numParams, numParamSets, maxFileSize, compressLevel)
{
  if (!sfstmt || !sfstmt->connection)
  {
    SNOWFLAKE_THROW("BindUploader:: Invalid statement");
  }
  SF_STATUS ret;
  m_stmt = snowflake_stmt(sfstmt->connection);
  if (sfstmt == NULL) {
    SET_SNOWFLAKE_ERROR(
      &sfstmt->error,
      SF_STATUS_ERROR_OUT_OF_MEMORY,
      "Out of memory in creating SF_STMT. ",
      SF_SQLSTATE_UNABLE_TO_CONNECT);

    SNOWFLAKE_THROW_S(&sfstmt->error);
  }
}

ClientBindUploader::~ClientBindUploader()
{
  if (m_stmt)
  {
    snowflake_stmt_term(m_stmt);
  }
}

bool ClientBindUploader::createStageIfNeeded()
{
  SF_CONNECT* conn = m_stmt->connection;
  // Check the flag without locking to get better performance.
  if (conn->binding_stage_created)
  {
    return true;
  }

  _mutex_lock(&conn->mutex_stage_bind);
  if (conn->binding_stage_created)
  {
    _mutex_unlock(&conn->mutex_stage_bind);
    return true;
  }

  std::string command = getCreateStageStmt();
  SF_STATUS ret = snowflake_query(m_stmt, command.c_str(), 0);
  if (ret != SF_STATUS_SUCCESS)
  {
    _mutex_unlock(&conn->mutex_stage_bind);
    std::string errorMessage("Bind uploading failed on creating internal stage: ");
    if (m_stmt->error.msg)
    {
      errorMessage += m_stmt->error.msg;
    }
    setError(errorMessage);
    return false;
  }

  conn->binding_stage_created = SF_BOOLEAN_TRUE;
  _mutex_unlock(&conn->mutex_stage_bind);

  return true;
}

bool ClientBindUploader::executeUploading(const std::string &sql,
                                          std::basic_iostream<char>& uploadStream,
                                          size_t dataSize)
{
  snowflake_prepare(m_stmt, sql.c_str(), 0);
  // No retry, fallback to regular binding is better than waste time on retry.
  SF_STATUS ret = _snowflake_execute_put_get_native(m_stmt, &uploadStream, dataSize, 1, NULL);
  if (ret != SF_STATUS_SUCCESS)
  {
    std::string errorMessage("Bind uploading failed: ");
    if (m_stmt->error.msg)
    {
      errorMessage += m_stmt->error.msg;
    }
    setError(errorMessage);
    return false;
  }

  return true;
}

} // namespace Client
} // namespace Snowflake

extern "C" {

using namespace Snowflake::Client;

char* STDCALL
_snowflake_stage_bind_upload(SF_STMT* sfstmt)
{
  std::string bindStage;
  ClientBindUploader uploader(sfstmt, sfstmt->request_id,
    sfstmt->params_len, sfstmt->paramset_size,
    SF_DEFAULT_STAGE_BINDING_MAX_FILESIZE, 0);

  const char* type;
  char name_buf[SF_PARAM_NAME_BUF_LEN];
  char* name = NULL;
  char* value = NULL;
  struct bind_info {
    SF_BIND_INPUT* input;
    void* val_ptr;
    int step;
  };
  std::vector<bind_info> bindInfo;
  for (unsigned int i = 0; i < sfstmt->params_len; i++)
  {
    SF_BIND_INPUT* input = _snowflake_get_binding_by_index(sfstmt, i, &name,
      name_buf, SF_PARAM_NAME_BUF_LEN);
    if (input == NULL)
    {
      log_error("_snowflake_execute_ex: No parameter by this name %s", name);
      return NULL;
    }
    bindInfo.emplace_back();
    bindInfo.back().input = input;
    bindInfo.back().val_ptr = input->value;
    bindInfo.back().step = _snowflake_get_binding_value_size(input);
  }
  for (int64 i = 0; i < sfstmt->paramset_size; i++)
  {
    for (unsigned int j = 0; j < sfstmt->params_len; j++)
    {
      SF_BIND_INPUT* input = bindInfo[j].input;
      void* val_ptr = bindInfo[j].val_ptr;
      int val_len = input->len;
      if (input->len_ind)
      {
        val_len = input->len_ind[i];
      }

      bool succeeded = false;
      if (SF_BIND_LEN_NULL == val_len)
      {
        succeeded = uploader.addNullValue();
      }
      else
      {
        if ((SF_C_TYPE_STRING == input->c_type) &&
            (SF_BIND_LEN_NTS == val_len))
        {
          val_len = strlen((char*)val_ptr);
        }

        value = value_to_string(val_ptr, val_len, input->c_type);
        if (value)
        {
          succeeded = uploader.addStringValue(value, input->type);
          SF_FREE(value);
        }
      }
      if (!succeeded)
      {
        CXX_LOG_WARN("stage bind uploading failed with error:%s. "
                     "Fallback to use regular binding. "
                     "Stage binding disabled for this connection.",
                     uploader.getError().c_str());
        sfstmt->connection->stage_binding_disabled = SF_BOOLEAN_TRUE;
      }
      bindInfo[j].val_ptr = (char*)bindInfo[j].val_ptr + bindInfo[j].step;
    }
  }
  bindStage = uploader.getStagePath();

  if (!bindStage.empty())
  {
    char* bind_stage = (char*) SF_CALLOC(1, bindStage.size() + 1);
    sf_strncpy(bind_stage, bindStage.size() + 1, bindStage.c_str(), bindStage.size());
    return bind_stage;
  }

  return NULL;
}

} // extern "C"
