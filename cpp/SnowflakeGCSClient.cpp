
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
  static const std::string GCS_TOKEN_KEY = "GCS_ACCESS_TOKEN";
  // followings are from GCS documentation, XML API
  // https://cloud.google.com/storage/docs
  static const std::string GCS_ENDPOINT = "storage.googleapis.com";
  static const std::string GCS_REGIONAL_ENDPOINT = "storage.%s.rep.googleapis.com";
  // me-central2 GCS region always use regional urls
  // TODO SNOW-1818804: the value is hardcoded now, but it should be server driven
  static const std::string GCS_REGION_ME_CENTRAL_2 = "me-central2";
  static const size_t URL_MAX_LEN = 2048;
  static const std::string GCS_AUTH_HEADER = "Authorization: Bearer ";
}

namespace Snowflake
{
  namespace Client
  {

    SnowflakeGCSClient::SnowflakeGCSClient(StageInfo *stageInfo, unsigned int parallel,
                                           TransferConfig *transferConfig, IStatementPutGet *statement) : m_stageInfo(stageInfo),
                                                                                                          m_statement(statement),
                                                                                                          m_gcsAccessToken(stageInfo ? stageInfo->credentials[GCS_TOKEN_KEY] : "")
    {
      if (!m_gcsAccessToken.empty())
      {
        CXX_LOG_INFO("Using GCS down scoped token.");
      }

      if (!m_stageInfo)
      {
        CXX_LOG_INFO("Internal error: invalid stage info");
        return;
      }

      // Ensure the stage location ended with /
      if ((!m_stageInfo->location.empty()) && (m_stageInfo->location.back() != '/'))
      {
        m_stageInfo->location.push_back('/');
      }

      m_stageEndpoint = GCS_ENDPOINT;
      if (!m_stageInfo->endPoint.empty())
      {
        m_stageEndpoint = m_stageInfo->endPoint;
      }
      else
      {
        bool isMeCentral2 = (0 == sf_strncasecmp(GCS_REGION_ME_CENTRAL_2.c_str(),
                                                 m_stageInfo->region.c_str(),
                                                 GCS_REGION_ME_CENTRAL_2.size()));
        if (isMeCentral2 || m_stageInfo->useRegionalUrl)
        {
          char buf[URL_MAX_LEN + 1];
          sf_sprintf(buf, sizeof(buf), GCS_REGIONAL_ENDPOINT.c_str(), m_stageInfo->region.c_str());
          m_stageEndpoint = buf;
        }
      }
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

      std::vector<std::string> reqHeaders;
      std::string respHeaders;
      std::string url;

      if (!m_gcsAccessToken.empty())
      {
        std::string filePathFull = m_stageInfo->location + fileMetadata->destFileName;

        // check if file exists if overwrite is not specified.
        if (!fileMetadata->overWrite)
        {
          RemoteStorageRequestOutcome outcome = GetRemoteFileMetadata(&filePathFull, fileMetadata);
          if (RemoteStorageRequestOutcome::SUCCESS == outcome)
          {
            CXX_LOG_DEBUG("File already exists skipping the file upload %s",
                          fileMetadata->srcFileToUpload.c_str());
            return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
          }
        }
        buildGcsRequest(filePathFull, url, reqHeaders);
      }
      else
      {
        url = fileMetadata->presignedUrl;
      }
      addUserMetadata(reqHeaders, fileMetadata);

      if (!m_statement->http_put(url,
                                 reqHeaders,
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
        std::basic_iostream<char> *dataStream)
    {
      CXX_LOG_DEBUG("Start download for file %s",
                    fileMetadata->srcFileName.c_str());

      std::string headerString;
      std::vector<std::string> reqHeaders;
      std::string url;

      if (!m_gcsAccessToken.empty())
      {
        buildGcsRequest(fileMetadata->srcFileName, url, reqHeaders);
      }
      else
      {
        url = fileMetadata->presignedUrl;
      }

      if (!m_statement->http_get(url,
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
      std::string url;

      if (!m_gcsAccessToken.empty())
      {
        buildGcsRequest(*filePathFull, url, reqHeaders);
      }
      else
      {
        url = fileMetadata->presignedUrl;
      }

      try
      {
        if (!m_statement->http_get(url,
                                   reqHeaders,
                                   NULL,
                                   headerString,
                                   true))
        {
          return RemoteStorageRequestOutcome::FAILED;
        }
      }
      catch (...)
      {
        // It's expected to fail when file doesn't exist, while http_get()
        // could throw excpetion on that. Catch that and return FAILED
        return RemoteStorageRequestOutcome::FAILED;
      }

      std::map<std::string, std::string> headers;
      parseHttpRespHeaders(headerString, headers);

      std::string key, iv;
      parseEncryptionMetadataFromJSON(headers[GCS_ENCRYPTIONDATAPROP], key, iv);
      Util::Base64::decode(iv.c_str(), iv.size(),
                           fileMetadata->encryptionMetadata.iv.data);
      fileMetadata->encryptionMetadata.enKekEncoded = key;

      fileMetadata->srcFileSize = strtoull(headers["Content-Length"].c_str(), NULL, 10);

      return RemoteStorageRequestOutcome::SUCCESS;
    }

    void SnowflakeGCSClient::addUserMetadata(
        std::vector<std::string> &userMetadata,
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

      userMetadata.push_back(GCS_ENCRYPTIONDATAPROP + ": " + buildEncryptionMetadataJSON(fileMetadata->encryptionMetadata.enKekEncoded, ivEncodedStr));
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
          std::string("{\"EncryptionMode\":\"FullBlob\",\"WrappedContentKey\"") + ":{\"KeyId\":\"symmKey1\",\"EncryptedKey\":\"" + key64 + "\"" + ",\"Algorithm\":\"AES_CBC_256\"},\"EncryptionAgent\":" + "{\"Protocol\":\"1.0\",\"EncryptionAlgorithm\":" + "\"AES_CBC_256\"},\"ContentEncryptionIV\":\"" + iv64 + "\"" + ",\"KeyWrappingMetadata\":{\"EncryptionLibrary\":" + "\"openssl 1.1.1i\"}}";

      return jsonStr;
    }

    void SnowflakeGCSClient::parseEncryptionMetadataFromJSON(std::string const &jsonStr,
                                                             std::string &key64,
                                                             std::string &iv64)
    {
      cJSON *json = snowflake_cJSON_Parse(jsonStr.c_str());
      cJSON *WrappedContentKey =
          snowflake_cJSON_GetObjectItem(json, "WrappedContentKey");
      cJSON *encryptedKey =
          snowflake_cJSON_GetObjectItem(WrappedContentKey, "EncryptedKey");
      cJSON *encryptedIv =
          snowflake_cJSON_GetObjectItem(json, "ContentEncryptionIV");

      if (encryptedKey)
      {
        key64 = encryptedKey->valuestring;
      }
      if (encryptedIv)
      {
        iv64 = encryptedIv->valuestring;
      }

      snowflake_cJSON_free(json);
    }

    void SnowflakeGCSClient::parseHttpRespHeaders(std::string const &headerString,
                                                  std::map<std::string, std::string> &headers)
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

    void SnowflakeGCSClient::buildGcsRequest(const std::string &filePathFull,
                                             std::string &url,
                                             std::vector<std::string> &reqHeaders)
    {
      reqHeaders.push_back(GCS_AUTH_HEADER + m_gcsAccessToken);

      std::string bucket;
      std::string object;
      extractBucketAndObject(filePathFull, bucket, object);
      object = encodeUrlName(object);

      // https://storage.googleapis.com//BUCKET_NAME/OBJECT_NAME
      url = "https://" + m_stageEndpoint + "/" + bucket + "/" + object;
      CXX_LOG_DEBUG("Build GCS request for file %s as URL: %s", filePathFull.c_str(), url.c_str());

      return;
    }

    void SnowflakeGCSClient::extractBucketAndObject(const std::string &fileFullPath,
                                                    std::string &bucket,
                                                    std::string &object)
    {
      size_t sepIndex = fileFullPath.find_first_of('/');
      bucket = fileFullPath.substr(0, sepIndex);
      object = fileFullPath.substr(sepIndex + 1);
    }

    std::string SnowflakeGCSClient::encodeUrlName(const std::string &srcName)
    {
      std::string encoded;
      char buf[5];
      buf[0] = '%';

      // encode all special characters
      for (size_t pos = 0; pos < srcName.length(); pos++)
      {
        // if unreserved, put as is
        // RFC 3986 section 2.3 Unreserved Characters
        unsigned char car = srcName[pos];
        if ((car >= '0' && car <= '9') ||
            (car >= 'A' && car <= 'Z') ||
            (car >= 'a' && car <= 'z') ||
            (car == '-' || car == '_' || car == '.' || car == '~'))
        {
          encoded.push_back(car);
        }
        else
        {
          sf_sprintf(&buf[1], sizeof(buf) - 1, "%.2X", car);
          encoded.append(buf);
        }
      }
      return encoded;
    }

  }
}
