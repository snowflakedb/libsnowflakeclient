/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <client_int.h>
#include "snowflake/PutGetParseResponse.hpp"
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

  SF_PUT_GET_RESPONSE *response = m_stmt->put_get_response;
  putGetParseResponse->parallel = (int)response->parallel;
  putGetParseResponse->autoCompress = (bool)response->auto_compress;
  putGetParseResponse->overwrite = (bool)response->overwrite;
  putGetParseResponse->clientShowEncryptionParameter = (bool)
    response->client_show_encryption_param;
  putGetParseResponse->sourceCompression = m_stmt->put_get_response
    ->source_compression;

  cJSON *src = (cJSON *) response->src_list;
  for (int i = 0; i < cJSON_GetArraySize(src); i++)
  {
    cJSON *val = cJSON_GetArrayItem(src, i);
    putGetParseResponse->srcLocations.emplace_back(val->valuestring);
  }

  if (sf_strncasecmp(response->command, "UPLOAD", 6) == 0)
  {
    putGetParseResponse->command = CommandType::UPLOAD;

    putGetParseResponse->encryptionMaterials.emplace_back(
      response->enc_mat_put->query_stage_master_key,
      response->enc_mat_put->query_id,
      response->enc_mat_put->smk_id);
  } else if (sf_strncasecmp(response->command, "DOWNLOAD", 8) == 0)
  {
    putGetParseResponse->command = CommandType::DOWNLOAD;
    cJSON *enc_mat_array_get = (cJSON *) response->enc_mat_get;
    for (int i=0; i<cJSON_GetArraySize(enc_mat_array_get); i++)
    {
      cJSON * enc_mat = cJSON_GetArrayItem(enc_mat_array_get, i);
      putGetParseResponse->encryptionMaterials.emplace_back(
        cJSON_GetObjectItem(enc_mat, "queryStageMasterKey")->valuestring,
        cJSON_GetObjectItem(enc_mat, "queryId")->valuestring,
        cJSON_GetObjectItem(enc_mat, "smkId")->valueint);
    }
  } else
  {
    putGetParseResponse->command = CommandType::UNKNOWN;
  }

  putGetParseResponse->localLocation = response->localLocation;

  putGetParseResponse->stageInfo.location = response->stage_info->location;
  putGetParseResponse->stageInfo.path = response->stage_info->path;
  putGetParseResponse->stageInfo.region = response->stage_info->region;
  putGetParseResponse->stageInfo.credentials = {
    {"AWS_KEY_ID",     response->stage_info->stage_cred->aws_key_id},
    {"AWS_SECRET_KEY", response->stage_info->stage_cred->aws_secret_key},
    {"AWS_TOKEN",      response->stage_info->stage_cred->aws_token}
  };

  if (sf_strncasecmp(response->stage_info->location_type, "s3", 2) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::S3;
  } else if (sf_strncasecmp(response->stage_info->location_type,
                            "azure", 5) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::AZURE;
  } else if (sf_strncasecmp(response->stage_info->location_type,
                            "local_fs", 8) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::LOCAL_FS;
  }
  return true;
}