//
// Created by hyu on 2/7/18.
//

#include "PutGetParseResponse.hpp"
#include "cJSON.h"

using namespace Snowflake::Client;

PutGetParseResponse::PutGetParseResponse(SF_PUT_GET_RESPONSE *put_get_response):
  m_parallel((int)put_get_response->parallel),
  m_autoCompress((bool)put_get_response->auto_compress),
  m_overwrite((bool)put_get_response->overwrite),
  m_clientShowEncryptionParameter((bool)put_get_response->client_show_encryption_param),
  m_sourceCompression(std::string(put_get_response->source_compression)),
  m_command(std::string(put_get_response->command))
{
  m_srcLocations = new std::vector<char *>;

  cJSON * src = (cJSON *)put_get_response->src_list;

  for (int i=0; i<cJSON_GetArraySize(src); i++)
  {
    cJSON *val = cJSON_GetArrayItem(src, i);
    m_srcLocations->push_back(val->valuestring);
  }

  m_encryptionMaterial = new EncryptionMaterial(
    put_get_response->enc_mat->query_stage_master_key,
    put_get_response->enc_mat->query_id,
    put_get_response->enc_mat->smk_id);

  m_stageInfo = new StageInfo(put_get_response->stage_info);
}

PutGetParseResponse::~PutGetParseResponse()
{
  delete m_srcLocations;
  delete m_encryptionMaterial;
  delete m_stageInfo;
}

