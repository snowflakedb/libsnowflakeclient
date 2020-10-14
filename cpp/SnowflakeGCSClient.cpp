/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#include "SnowflakeGCSClient.hpp"
#include "FileMetadataInitializer.hpp"
#include "EncryptionProvider.hpp"
#include "util/Base64.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include "logger/SFLogger.hpp"
#include "cJSON.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace
{
  static const std::string GCS_ENCRYPTIONDATAPROP = "x-goog-meta-encryptiondata";
  static const std::string GCS_MATDESC = "x-goog-meta-matdesc";
  static const std::string SFC_DIGEST = "sfc-digest";
}

namespace Snowflake
{
namespace Client
{

SnowflakeGCSClient::SnowflakeGCSClient(StageInfo *stageInfo, unsigned int parallel,
  TransferConfig * transferConfig, IStatementPutGet* statement) :
  m_stageInfo(stageInfo),
  m_statement(statement)
{
  // Do nothing.
}

SnowflakeGCSClient::~SnowflakeGCSClient()
{
  // Do nothing;
}

RemoteStorageRequestOutcome SnowflakeGCSClient::upload(FileMetadata *fileMetadata,
                                          std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start upload for file %s",
    fileMetadata->srcFileToUpload.c_str());

  std::vector<std::string> userMetadata;
  addUserMetadata(userMetadata, fileMetadata);
  std::string respHeaders;

  if (!m_statement->http_put(fileMetadata->presignedUrl,
                             userMetadata,
                             *dataStream,
                             fileMetadata->destFileSize,
                             respHeaders))
  {
    return RemoteStorageRequestOutcome::FAILED;
  }

  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeGCSClient::download(
  FileMetadata *fileMetadata,
  std::basic_iostream<char>* dataStream)
{
  CXX_LOG_DEBUG("Start download for file %s",
    fileMetadata->srcFileName.c_str());

  std::string headerString;
  std::vector<std::string> reqHeaders;

  if (!m_statement->http_get(fileMetadata->presignedUrl,
                             reqHeaders,
                             dataStream,
                             headerString,
                             false))
  {
    return RemoteStorageRequestOutcome::FAILED;
  }

  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeGCSClient::GetRemoteFileMetadata(
  std::string *filePathFull, FileMetadata *fileMetadata)
{
  std::string headerString;
  std::vector<std::string> reqHeaders;

  if (!m_statement->http_get(fileMetadata->presignedUrl,
                             reqHeaders,
                             NULL,
                             headerString,
                             true))
  {
    return RemoteStorageRequestOutcome::FAILED;
  }

  std::map<std::string, std::string> headers;
  parseHttpRespHeaders(headerString, headers);

  std::string key, iv;
  parseEncryptionMetadataFromJSON(headers[GCS_ENCRYPTIONDATAPROP], key, iv);
  Util::Base64::decode(iv.c_str(), iv.size(),
                       fileMetadata->encryptionMetadata.iv.data);
  fileMetadata->encryptionMetadata.enKekEncoded = key;

  fileMetadata->srcFileSize = strtol(headers["Content-Length"].c_str(), NULL, 10);

  return RemoteStorageRequestOutcome::SUCCESS;
}

void SnowflakeGCSClient::addUserMetadata(
  std::vector<std::string>& userMetadata,
  FileMetadata *fileMetadata)
{
  userMetadata.push_back("content-encoding: ");
  userMetadata.push_back(SFC_DIGEST + ": " + fileMetadata->sha256Digest);
  userMetadata.push_back(GCS_MATDESC + ": " + fileMetadata->encryptionMetadata.matDesc);

  char ivEncoded[64];
  Snowflake::Client::Util::Base64::encode(
    fileMetadata->encryptionMetadata.iv.data,
    Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES),
    ivEncoded);
  size_t ivEncodeSize = Snowflake::Client::Util::Base64::encodedLength(
    Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES));
  std::string ivEncodedStr(ivEncoded, ivEncodeSize);

  userMetadata.push_back(GCS_ENCRYPTIONDATAPROP + ": " + buildEncryptionMetadataJSON(
    fileMetadata->encryptionMetadata.enKekEncoded,  ivEncodedStr));
}

/*
* buildEncryptionMetadataJSON
* Takes the base64-encoded iv and key and creates the JSON block to be
* used as the encryptiondata metadata field on the blob.
*/
std::string SnowflakeGCSClient::buildEncryptionMetadataJSON(std::string key64,
                                                            std::string iv64)
{
  std::string jsonStr = 
    std::string("{\"EncryptionMode\":\"FullBlob\",\"WrappedContentKey\"")
    + ":{\"KeyId\":\"symmKey1\",\"EncryptedKey\":\"" + key64 + "\""
    + ",\"Algorithm\":\"AES_CBC_256\"},\"EncryptionAgent\":"
    + "{\"Protocol\":\"1.0\",\"EncryptionAlgorithm\":"
    + "\"AES_CBC_256\"},\"ContentEncryptionIV\":\"" + iv64 + "\""
    + ",\"KeyWrappingMetadata\":{\"EncryptionLibrary\":"
    + "\"openssl 1.1.1g\"}}";

  return jsonStr;
}

void SnowflakeGCSClient::parseEncryptionMetadataFromJSON(std::string const& jsonStr,
                                                         std::string& key64,
                                                         std::string& iv64)
{
  cJSON* json = snowflake_cJSON_Parse(jsonStr.c_str());
  cJSON* WrappedContentKey =
          snowflake_cJSON_GetObjectItem(json, "WrappedContentKey");
  cJSON* encryptedKey = 
          snowflake_cJSON_GetObjectItem(WrappedContentKey, "EncryptedKey");
  cJSON* encryptedIv =
          snowflake_cJSON_GetObjectItem(json, "ContentEncryptionIV");

  key64 = encryptedKey->valuestring;
  iv64 = encryptedIv->valuestring;

  snowflake_cJSON_free(json);
}

void SnowflakeGCSClient::parseHttpRespHeaders(std::string const& headerString,
                            std::map<std::string, std::string>& headers)
{
  std::string header;
  std::stringstream headerStream(headerString);
  size_t index;
  while (std::getline(headerStream, header) && header != "\r")
  {
    index = header.find(':', 0);
    if (index != std::string::npos)
    {
      std::string key, value;
      key = header.substr(0, index);
      value = header.substr(index + 1);
      headers[key] = value;
    }
  }
}

}
}
