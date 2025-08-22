#include <client_int.h>
#include "connection.h"
#include "snowflake/PutGetParseResponse.hpp"
#include "StatementPutGet.hpp"
#include "curl_desc_pool.h"

using namespace Snowflake::Client;

static size_t file_get_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  size_t data_size = size * nmemb;
  std::basic_iostream<char>* recvStream = (std::basic_iostream<char>*)(userdata);
  if (recvStream)
  {
    recvStream->write(static_cast<const char*>(ptr), data_size);
  }

  return data_size;
}

static size_t file_put_read_callback(void* ptr, size_t size, size_t nmemb, void* userdata)
{
  std::basic_iostream<char>* payload = (std::basic_iostream<char>*)(userdata);
  size_t data_size = size * nmemb;

  payload->read(static_cast<char*>(ptr), data_size);
  size_t ret = payload->gcount();
  return payload->gcount();
}

StatementPutGet::StatementPutGet(SF_STMT *stmt) :
  m_stmt(stmt), m_useProxy(false)
{
  if (m_stmt && m_stmt->connection && m_stmt->connection->proxy)
  {
    m_useProxy = true;
    m_proxy = Util::Proxy(std::string(m_stmt->connection->proxy));
    if (m_stmt->connection->no_proxy)
    {
      m_proxy.setNoProxy(std::string(m_stmt->connection->no_proxy));
    }
  }
}

bool StatementPutGet::parsePutGetCommand(std::string *sql,
                                         PutGetParseResponse *putGetParseResponse)
{
  if (_snowflake_query_put_get_legacy(m_stmt, sql->c_str(), 0) != SF_STATUS_SUCCESS)
  {
    return false;
  }

  SF_PUT_GET_RESPONSE *response = m_stmt->put_get_response;
  putGetParseResponse->parallel = (int)response->parallel;
  putGetParseResponse->autoCompress = (bool)response->auto_compress;
  putGetParseResponse->overwrite = (bool)response->overwrite;
  putGetParseResponse->clientShowEncryptionParameter = (bool)
    response->client_show_encryption_param;
  putGetParseResponse->sourceCompression = m_stmt->put_get_response
    ->source_compression;

  cJSON *src = (cJSON *) response->src_list;
  int src_size = snowflake_cJSON_GetArraySize(src);
  for (int i = 0; i < src_size; i++)
  {
    cJSON *val = snowflake_cJSON_GetArrayItem(src, i);
    putGetParseResponse->srcLocations.emplace_back(val->valuestring);
  }

  if (sf_strncasecmp(response->command, "UPLOAD", 6) == 0)
  {
    putGetParseResponse->command = CommandType::UPLOAD;

    putGetParseResponse->threshold = (size_t)response->threshold;
    if (response->enc_mat_put->query_stage_master_key)
    {
      putGetParseResponse->encryptionMaterials.emplace_back(
        response->enc_mat_put->query_stage_master_key,
        response->enc_mat_put->query_id,
        response->enc_mat_put->smk_id);
    }
  } else if (sf_strncasecmp(response->command, "DOWNLOAD", 8) == 0)
  {
    putGetParseResponse->command = CommandType::DOWNLOAD;
    cJSON *enc_mat_array_get = (cJSON *) response->enc_mat_get;
    int enc_mat_array_size = snowflake_cJSON_GetArraySize(enc_mat_array_get);
    for (int i = 0; i < enc_mat_array_size; i++)
    {
      cJSON * enc_mat = snowflake_cJSON_GetArrayItem(enc_mat_array_get, i);
      putGetParseResponse->encryptionMaterials.emplace_back(
        snowflake_cJSON_GetObjectItem(enc_mat, "queryStageMasterKey")->valuestring,
        snowflake_cJSON_GetObjectItem(enc_mat, "queryId")->valuestring,
        snowflake_cJSON_GetObjectItem(enc_mat, "smkId")->valueint);
    }
  } else
  {
    putGetParseResponse->command = CommandType::UNKNOWN;
  }

  putGetParseResponse->localLocation = response->localLocation;

  putGetParseResponse->stageInfo.location = response->stage_info->location;
  putGetParseResponse->stageInfo.path = response->stage_info->path;

  if (response->stage_info->region != NULL)
  {
    putGetParseResponse->stageInfo.region = response->stage_info->region;
  }
  if (response->stage_info->endPoint != NULL) {
    putGetParseResponse->stageInfo.endPoint = response->stage_info->endPoint;
  }
  putGetParseResponse->stageInfo.useRegionalUrl = response->stage_info->useRegionalUrl;
  if (sf_strncasecmp(response->stage_info->location_type, "s3", 2) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::S3;
    putGetParseResponse->stageInfo.useS3RegionalUrl = (SF_BOOLEAN_TRUE == response->stage_info->useS3RegionalUrl);
    putGetParseResponse->stageInfo.credentials = {
            {"AWS_KEY_ID",     response->stage_info->stage_cred->aws_key_id},
            {"AWS_SECRET_KEY", response->stage_info->stage_cred->aws_secret_key},
            {"AWS_TOKEN",      response->stage_info->stage_cred->aws_token}
      };
  } else if (sf_strncasecmp(response->stage_info->location_type, "azure", 5) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::AZURE;
    putGetParseResponse->stageInfo.storageAccount= response->stage_info->storageAccount;
    putGetParseResponse->stageInfo.credentials = {
            {"AZURE_SAS_KEY",     response->stage_info->stage_cred->azure_sas_token}
      };
  }
  else if (sf_strncasecmp(response->stage_info->location_type, "gcs", 3) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::GCS;
    putGetParseResponse->stageInfo.useVirtualUrl = (SF_BOOLEAN_TRUE == response->stage_info->useVirtualUrl);
    putGetParseResponse->stageInfo.credentials = {
            {"GCS_ACCESS_TOKEN",     response->stage_info->stage_cred->gcs_access_token}
    };

  } else if (sf_strncasecmp(response->stage_info->location_type,
                            "local_fs", 8) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::LOCAL_FS;
  }
  return true;
}

Util::Proxy* StatementPutGet::get_proxy()
{
  if (!m_useProxy)
  {
    return NULL;
  }
  else
  {
    return &m_proxy;
  }
}

bool StatementPutGet::http_put(std::string const& url,
                               std::vector<std::string> const& headers,
                               std::basic_iostream<char>& payload,
                               size_t payloadLen,
                               std::string& responseHeaders)
{
  if (!m_stmt || !m_stmt->connection)
  {
    return false;
  }
  SF_CONNECT* sf = m_stmt->connection;
  void* curl_desc = get_curl_desc_from_pool(url.c_str(), sf->proxy, sf->no_proxy);
  CURL* curl = get_curl_from_desc(curl_desc);
  if (!curl)
  {
    return false;
  }

  char* urlbuf = (char*)SF_CALLOC(1, url.length() + 1);
  sf_strcpy(urlbuf, url.length() + 1, url.c_str());

  SF_HEADER reqHeaders;
  reqHeaders.header = NULL;
  for (auto itr = headers.begin(); itr != headers.end(); itr++)
  {
    reqHeaders.header = curl_slist_append(reqHeaders.header, itr->c_str());
  }

  PUT_PAYLOAD putPayload;
  putPayload.buffer = &payload;
  putPayload.length = payloadLen;
  putPayload.read_callback = file_put_read_callback;

  char* respHeaders = NULL;
  sf_bool success = SF_BOOLEAN_FALSE;

  success = http_perform(curl, PUT_REQUEST_TYPE, urlbuf, &reqHeaders, NULL, &putPayload, NULL,
                         NULL, &respHeaders, get_retry_timeout(sf),
                         SF_BOOLEAN_FALSE, &m_stmt->error, sf->insecure_mode,sf->ocsp_fail_open,
                         sf->clr_check, sf->retry_on_curle_couldnt_connect_count,
                         0, sf->retry_count, NULL, NULL, NULL, SF_BOOLEAN_FALSE,
                         sf->proxy, sf->no_proxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);

  free_curl_desc(curl_desc);
  SF_FREE(urlbuf);
  curl_slist_free_all(reqHeaders.header);
  if (respHeaders)
  {
    responseHeaders = std::string(respHeaders);
    SF_FREE(respHeaders);
  }

  return success;
}

bool StatementPutGet::http_get(std::string const& url,
                               std::vector<std::string> const& headers,
                               std::basic_iostream<char>* payload,
                               std::string& responseHeaders,
                               bool headerOnly)
{
  SF_REQUEST_TYPE reqType = GET_REQUEST_TYPE;
  if (headerOnly)
  {
    reqType = HEAD_REQUEST_TYPE;
  }

  if (!m_stmt || !m_stmt->connection)
  {
    return false;
  }
  SF_CONNECT* sf = m_stmt->connection;

  void* curl_desc = get_curl_desc_from_pool(url.c_str(), sf->proxy, sf->no_proxy);
  CURL* curl = get_curl_from_desc(curl_desc);
  if (!curl)
  {
    return false;
  }

  char* urlbuf = (char*)SF_CALLOC(1, url.length() + 1);
  sf_strcpy(urlbuf, url.length() + 1, url.c_str());

  SF_HEADER reqHeaders;
  reqHeaders.header = NULL;
  for (auto itr = headers.begin(); itr != headers.end(); itr++)
  {
    reqHeaders.header = curl_slist_append(reqHeaders.header, itr->c_str());
  }

  NON_JSON_RESP resp;
  resp.buffer = payload;
  resp.write_callback = file_get_write_callback;

  char* respHeaders = NULL;
  sf_bool success = SF_BOOLEAN_FALSE;

  success = http_perform(curl, reqType, urlbuf, &reqHeaders, NULL, NULL, NULL,
                         &resp, &respHeaders, get_retry_timeout(sf),
                         SF_BOOLEAN_FALSE, &m_stmt->error, sf->insecure_mode, sf->ocsp_fail_open,
                         sf->clr_check, sf->retry_on_curle_couldnt_connect_count,
                         0, sf->retry_count, NULL, NULL, NULL, SF_BOOLEAN_FALSE,
                         sf->proxy, sf->no_proxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);

  free_curl_desc(curl_desc);
  SF_FREE(urlbuf);
  curl_slist_free_all(reqHeaders.header);
  if (respHeaders)
  {
    responseHeaders = respHeaders;
    SF_FREE(respHeaders);
  }

  if (payload)
  {
    payload->flush();
  }

  return success;
}
