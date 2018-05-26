/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/PutGetParseResponse.hpp"
#include "snowflake/platform.h"
#include "cJSON.h"

using namespace Snowflake::Client;

PutGetParseResponse::PutGetParseResponse(SF_PUT_GET_RESPONSE *put_get_response)
  :
  m_parallel((int) put_get_response->parallel),
  m_autoCompress((bool) put_get_response->auto_compress),
  m_overwrite((bool) put_get_response->overwrite),
  m_clientShowEncryptionParameter(
    (bool) put_get_response->client_show_encryption_param),
  m_sourceCompression(put_get_response->source_compression)
{

  cJSON *src = (cJSON *) put_get_response->src_list;

  for (int i = 0; i < cJSON_GetArraySize(src); i++)
  {
    cJSON *val = cJSON_GetArrayItem(src, i);
    m_srcLocations.emplace_back(val->valuestring);
  }

  char *commandStr = put_get_response->command;
  if (sf_strncasecmp(commandStr, "UPLOAD", 6) == 0)
  {
    m_command = CommandType::UPLOAD;

    m_encryptionMaterials.emplace_back();
    m_encryptionMaterials.back().queryStageMasterKey = std::string(
      put_get_response->enc_mat_put->query_stage_master_key);
    m_encryptionMaterials.back().queryId = std::string(
      put_get_response->enc_mat_put->query_id);
    m_encryptionMaterials.back().smkId = put_get_response->enc_mat_put->smk_id;
  } else if (sf_strncasecmp(commandStr, "DOWNLOAD", 8) == 0)
  {
    m_command = CommandType::DOWNLOAD;
    cJSON *enc_mat_array_get = (cJSON *) put_get_response->enc_mat_get;
    for (int i=0; i<cJSON_GetArraySize(enc_mat_array_get); i++)
    {
      cJSON * enc_mat = cJSON_GetArrayItem(enc_mat_array_get, i);
      m_encryptionMaterials.emplace_back();
      m_encryptionMaterials.back().queryStageMasterKey = std::string(
        cJSON_GetObjectItem(enc_mat, "queryStageMasterKey")->valuestring);
      m_encryptionMaterials.back().queryId = std::string(
        cJSON_GetObjectItem(enc_mat, "queryId")->valuestring);
      m_encryptionMaterials.back().smkId = cJSON_GetObjectItem(enc_mat, "smkId")->valueint;
    }
  } else
  {
    m_command = CommandType::UNKNOWN;
  }

  m_stageInfo = StageInfo(put_get_response->stage_info);


  m_localLocation = put_get_response->localLocation;
}

void PutGetParseResponse::updateWith(SF_PUT_GET_RESPONSE *put_get_response)
{
  *this = PutGetParseResponse(put_get_response);
}

