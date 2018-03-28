//
// Created by hyu on 2/7/18.
//

#include "PutGetParseResponse.hpp"
#include "cJSON.h"

using namespace Snowflake::Client;

PutGetParseResponse::PutGetParseResponse(SF_PUT_GET_RESPONSE *put_get_response)
  :
  m_parallel((int) put_get_response->parallel),
  m_autoCompress((bool) put_get_response->auto_compress),
  m_overwrite((bool) put_get_response->overwrite),
  m_clientShowEncryptionParameter(
    (bool) put_get_response->client_show_encryption_param)
{

  cJSON *src = (cJSON *) put_get_response->src_list;

  for (int i = 0; i < cJSON_GetArraySize(src); i++)
  {
    cJSON *val = cJSON_GetArrayItem(src, i);
    m_srcLocations.push_back(std::string(val->valuestring));
  }

  m_encryptionMaterial.queryStageMasterKey = std::string(
    put_get_response->enc_mat->query_stage_master_key);
  m_encryptionMaterial.queryId = std::string(
    put_get_response->enc_mat->query_id);
  m_encryptionMaterial.smkId = put_get_response->enc_mat->smk_id;

  m_stageInfo = StageInfo(put_get_response->stage_info);

  char *commandStr = put_get_response->command;
  if (strncasecmp(commandStr, "UPLOAD", 6) == 0)
  {
    m_command = CommandType::UPLOAD;
  } else if (strncasecmp(commandStr, "DOWNLOAD", 8) == 0)
  {
    m_command = CommandType::DOWNLOAD;
  } else
  {
    m_command = CommandType::UNKNOWN;
  }

  char *sourceCompression = put_get_response->source_compression;
  if (!strncasecmp(sourceCompression, "AUTO_DETECT", 11))
  {
    m_sourceCompression = CompressionType::AUTO_DETECT;
  } else if (!strncasecmp(sourceCompression, "GZIP", 4))
  {
    m_sourceCompression = CompressionType::GZIP;
  } else if (!strncasecmp(sourceCompression, "NONE", 4))
  {
    m_sourceCompression = CompressionType::NONE;
  } else
  {
    m_sourceCompression = CompressionType::NOT_SUPPORTED;
  }

}

void PutGetParseResponse::updateWith(SF_PUT_GET_RESPONSE *put_get_response)
{
  *this = PutGetParseResponse(put_get_response);
}

