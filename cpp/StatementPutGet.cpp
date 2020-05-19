/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <client_int.h>
#include "snowflake/PutGetParseResponse.hpp"
#include "StatementPutGet.hpp"

using namespace Snowflake::Client;

namespace
{
  /**
  * @brief read callback function for libcurl
  *
  * @param data
  *   pointer to a buffer to write data into. The maximum size of that buffer
  *   should be  (size*nmemb)
  *
  * @param size
  *   size parameter
  *
  * @param nmemb
  *   memblock parameter
  *
  * @param userdata
  *   pointer to user data to read data from
  *
  * @return (size * nmemb)
  */
  size_t doRead(void *data,
                size_t size,
                size_t nmemb,
                void *userdata)
  {
    /** get POST/PUT payload (passed to us by libcurl) */
    std::basic_iostream<char>* payload =
      reinterpret_cast<std::basic_iostream<char> *> (userdata);

    size_t curl_size = size * nmemb;

    payload->read(static_cast<char*>(data), curl_size);
    return payload->gcount();
  }

  /**
  * @brief write callback function for libcurl
  *   This callback append to the stream of the response to be returned to the
  *   invoker of the REST call.
  *
  * @param data
  *   returned data of size (size*nmemb)
  *
  * @param size
  *   size parameter
  *
  * @param nmemb
  *   memblock parameter
  *
  * @param userdata
  *   pointer to user data to save/work with return data
  *
  * @return (size * nmemb)
  */
  size_t doWriteStream(void  *data,
                       size_t size,
                       size_t nmemb,
                       void *userdata)
  {
    // total number of bytes received
    size_t totalBytes = size * nmemb;

    // Cast back the userdata as a stream
    std::basic_iostream<char>* payload =
      reinterpret_cast<std::basic_iostream<char> *> (userdata);

    // Append what we received in the response body
    payload->write(static_cast<const char*>(data), totalBytes);

    return totalBytes;
  }

  /**
  * @brief header callback function for libcurl
  *   This callback append to the string of the response to be returned to the
  *   invoker of the REST call.
  *
  * @param data
  *   returned data of size (size*nmemb)
  *
  * @param size
  *   size parameter
  *
  * @param nmemb
  *   memblock parameter
  *
  * @param userdata
  *   pointer to user data to save/work with return data
  *
  * @return (size * nmemb)
  */
  size_t doWriteString(void  *data,
                       size_t size,
                       size_t nmemb,
                       void *userdata)
  {
    // total number of bytes received
    size_t totalBytes = size * nmemb;

    // Cast back the userdata as a stream
    std::string* headers =
      reinterpret_cast<std::string *> (userdata);

    // Append what we received in the response header
    headers->append(static_cast<const char*>(data), totalBytes);

    return totalBytes;
  }
}

StatementPutGet::StatementPutGet(SF_STMT *stmt) :
  m_stmt(stmt)
{
}

bool StatementPutGet::parsePutGetCommand(std::string *sql,
                                         PutGetParseResponse *putGetParseResponse)
{
  if (snowflake_query(m_stmt, sql->c_str(), 0) != SF_STATUS_SUCCESS)
  {
    return false;
  }

  putGetParseResponse->encryptionMaterials.clear();
  putGetParseResponse->srcLocations.clear();
  putGetParseResponse->presignedUrls.clear();

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
    putGetParseResponse->encryptionMaterials.emplace_back(
      response->enc_mat_put->query_stage_master_key,
      response->enc_mat_put->query_id,
      response->enc_mat_put->smk_id);
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

    cJSON *presignedUrls = (cJSON *)response->presignedUrls;
    int presignedUrlArraySize = presignedUrls ? 
                                snowflake_cJSON_GetArraySize(presignedUrls) : 0;
    for (int i = 0; i < presignedUrlArraySize; i++)
    {
      cJSON * url = snowflake_cJSON_GetArrayItem(presignedUrls, i);
      putGetParseResponse->presignedUrls.emplace_back(url->valuestring);
    }
  } else
  {
    putGetParseResponse->command = CommandType::UNKNOWN;
  }

  putGetParseResponse->localLocation = response->localLocation;

  putGetParseResponse->stageInfo.location = response->stage_info->location;
  putGetParseResponse->stageInfo.path = response->stage_info->path;

  if (sf_strncasecmp(response->stage_info->location_type, "s3", 2) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::S3;
    putGetParseResponse->stageInfo.region = response->stage_info->region;
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
    putGetParseResponse->stageInfo.endPoint = response->stage_info->endPoint;

  }
  else if (sf_strncasecmp(response->stage_info->location_type, "gcs", 3) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::GCS;
    if (response->stage_info->presignedUrl)
    {
      putGetParseResponse->stageInfo.presignedUrl = response->stage_info->presignedUrl;
    }

  } else if (sf_strncasecmp(response->stage_info->location_type,
                            "local_fs", 8) == 0)
  {
    putGetParseResponse->stageInfo.stageType = StageType::LOCAL_FS;
  }
  return true;
}

bool StatementPutGet::http_put(std::string const& url,
                               std::vector<std::string> const& headers,
                               std::basic_iostream<char>& payload,
                               size_t payloadLen,
                               std::string& responseHeaders)
{
  return http_perform(PUT_REQUEST_TYPE, url, headers,
                      &payload, payloadLen, responseHeaders);
}

bool StatementPutGet::http_get(std::string const& url,
                               std::vector<std::string> const& headers,
                               std::basic_iostream<char>* payload,
                               std::string& responseHeaders,
                               bool headerOnly)
{
  bool ret = http_perform(GET_REQUEST_TYPE, url, headers,
                          payload, 0, responseHeaders, headerOnly);

  if (payload && ret)
  {
    payload->flush();
  }

  return ret;
}

bool StatementPutGet::http_perform(SF_REQUEST_TYPE request_type,
                                   std::string const& url,
                                   std::vector<std::string> const& headers,
                                   std::basic_iostream<char>* payload,
                                   size_t payloadLen,
                                   std::string& responseHeaders,
                                   bool headerOnly)
{
  CURLcode res;
  bool ret = false;
  sf_bool retry = false;
  long int http_code = 0;

  if ((PUT_REQUEST_TYPE != request_type) && (GET_REQUEST_TYPE != request_type))
  {
    return false;
  }
  if ((PUT_REQUEST_TYPE == request_type) && (NULL == payload))
  {
    return false;
  }

  CURL* curl = curl_easy_init();
  if (curl == NULL) {
    return false;
  }

  do {
    // Set parameters
    res = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (res != CURLE_OK) {
      log_error("Failed to set URL [%s]", curl_easy_strerror(res));
      break;
    }

    // set headers
    struct curl_slist * headerList = NULL;
    if (headers.size() > 0) {
      for (std::vector<std::string>::const_iterator itr = headers.begin();
           itr != headers.end(); itr++)
      {
        headerList = curl_slist_append(headerList, itr->c_str());
      }
      res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
      if (res != CURLE_OK) {
        log_error("Failed to set header [%s]", curl_easy_strerror(res));
        break;
      }
    }

    // set response header callback function
    res = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, doWriteString);
    if (res != CURLE_OK) {
      log_error("Failed to set header writer [%s]", curl_easy_strerror(res));
      break;
    }

    res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&responseHeaders);
    if (res != CURLE_OK) {
      log_error("Failed to set header write data [%s]", curl_easy_strerror(res));
      break;
    }

    if (headerOnly)
    {
      /** we want response header only */
      res = curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
      if (res != CURLE_OK) {
        log_error("Failed to set header write data [%s]", curl_easy_strerror(res));
        break;
      }
    }

    // set callback function for response headers
    res = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, doWriteString);
    if (res != CURLE_OK) {
      log_error("Failed to set header writer [%s]", curl_easy_strerror(res));
      break;
    }

    res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&responseHeaders);
    if (res != CURLE_OK) {
      log_error("Failed to set header data [%s]", curl_easy_strerror(res));
      break;
    }

    // request specific settings
    if (PUT_REQUEST_TYPE == request_type)
    {
      res = curl_easy_setopt(curl, CURLOPT_PUT, 1L);
      if (res != CURLE_OK) {
        log_error("Failed to set put request [%s]", curl_easy_strerror(res));
        break;
      }

      res = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
      if (res != CURLE_OK) {
        log_error("Failed to set upload [%s]", curl_easy_strerror(res));
        break;
      }

      /** set read callback function */
      res = curl_easy_setopt(curl, CURLOPT_READFUNCTION, doRead);
      if (res != CURLE_OK) {
        log_error("Failed to set reader [%s]", curl_easy_strerror(res));
        break;
      }

      /** set data object to pass to callback function */
      res = curl_easy_setopt(curl, CURLOPT_READDATA, (void *)payload);
      if (res != CURLE_OK) {
        log_error("Failed to set read data [%s]", curl_easy_strerror(res));
        break;
      }

      /** set size */
      res = curl_easy_setopt(curl, CURLOPT_INFILESIZE, payloadLen);
      if (res != CURLE_OK) {
        log_error("Failed to set payload size [%s]", curl_easy_strerror(res));
        break;
      }
    }
    else if ((GET_REQUEST_TYPE == request_type) && (!headerOnly))
    {
      res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, doWriteStream);
      if (res != CURLE_OK) {
        log_error("Failed to set writer [%s]", curl_easy_strerror(res));
        break;
      }

      res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)payload);
      if (res != CURLE_OK) {
        log_error("Failed to set write data [%s]", curl_easy_strerror(res));
        break;
      }
    }

    if (DISABLE_VERIFY_PEER) {
      res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      if (res != CURLE_OK) {
        log_error("Failed to disable peer verification [%s]",
          curl_easy_strerror(res));
        break;
      }
    }

    res = curl_easy_setopt(curl, CURLOPT_CAINFO, m_caBundle.c_str());
    if (res != CURLE_OK) {
      log_error("Unable to set certificate file [%s]",
        curl_easy_strerror(res));
      break;
    }

    res = curl_easy_setopt(curl, CURLOPT_SSLVERSION, SSL_VERSION);
    if (res != CURLE_OK) {
      log_error("Unable to set SSL Version [%s]",
        curl_easy_strerror(res));
      break;
    }

    // Be optimistic
    retry = SF_BOOLEAN_FALSE;

    log_trace("Running curl call");
    res = curl_easy_perform(curl);
    /* Check for errors */
    if (res != CURLE_OK) {
      char msg[1024];
      if (res == CURLE_SSL_CACERT_BADFILE) {
        sb_sprintf(msg, sizeof(msg), "curl_easy_perform() failed. err: %s, CA Cert file: %s",
          curl_easy_strerror(res), CA_BUNDLE_FILE ? CA_BUNDLE_FILE : "Not Specified");
      }
      else {
        sb_sprintf(msg, sizeof(msg), "curl_easy_perform() failed: %s", curl_easy_strerror(res));
      }
    }
    else {
      if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code) != CURLE_OK) {
        log_error("Unable to get http response code [%s]",
          curl_easy_strerror(res));
      }
      else if (http_code != 200) {
        std::cout << "error http code is " << http_code << std::endl;
        retry = is_retryable_http_code(http_code);
        if (!retry) {
          log_error("Received unretryable http code [%d]", http_code);
        }
      }
      else {
        ret = true;
      }
    }

    // Reset everything
    if (headerList)
    {
      curl_slist_free_all(headerList);
      headerList = NULL;
    }
    reset_curl(curl);
    http_code = 0;
  } while (retry);

  // clear curl descriptor
  if (curl)
  {
    curl_easy_cleanup(curl);
  }

  return ret;
}