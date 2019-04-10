/*
 * Copyright (c) 2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef _WIN32
//Azure put get is supported in Linux only

#include "SnowflakeAzureClient.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/client.h"
#include "util/Base64.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include "util/Proxy.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include "logger/SFAwsLogger.hpp"
#include "logger/SFLogger.hpp"
#include "SnowflakeS3Client.hpp"
#include "storage_credential.h"
#include "storage_account.h"
#include "blob/blob_client.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>



#define CONTENT_TYPE_OCTET_STREAM "application/octet-stream"
#define SFC_DIGEST "sfc-digest"
#define AMZ_KEY "x-amz-key"
#define AMZ_IV "x-amz-iv"
#define AMZ_MATDESC "x-amz-matdesc"

namespace Snowflake
{
namespace Client
{


SnowflakeAzureClient::SnowflakeAzureClient(StageInfo *stageInfo, unsigned int parallel,
  TransferConfig * transferConfig):
  m_stageInfo(stageInfo),
  m_threadPool(nullptr),
  m_parallel(std::min(parallel, std::thread::hardware_concurrency()))
{
  const std::string azuresaskey("AZURE_SAS_KEY");
  char caBundleFile[256] = {0};

  snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, caBundleFile);
  CXX_LOG_TRACE("ca bundle file from SF_GLOBAL_CA_BUNDLE_FILE *%s*", caBundleFile);

  std::string account_name = m_stageInfo->storageAccount;
  std::string sas_key = m_stageInfo->credentials[azuresaskey];
  std::string endpoint = account_name + "." + m_stageInfo->endPoint;
  std::shared_ptr<azure::storage_lite::storage_credential>  cred = std::make_shared<azure::storage_lite::shared_access_signature_credential>(sas_key);
  std::shared_ptr<azure::storage_lite::storage_account> account = std::make_shared<azure::storage_lite::storage_account>(account_name, cred, true, endpoint);
  auto bc = std::make_shared<azure::storage_lite::blob_client>(account, parallel, caBundleFile);
  m_blobclient= new azure::storage_lite::blob_client_wrapper(bc);

  CXX_LOG_TRACE("Successfully created Azure client. End of constructor.");
}

SnowflakeAzureClient::~SnowflakeAzureClient()
{
}

RemoteStorageRequestOutcome SnowflakeAzureClient::upload(FileMetadata *fileMetadata,
                                          std::basic_iostream<char> *dataStream)
{
    return doSingleUpload(fileMetadata, dataStream);
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doSingleUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start single part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());

  std::string containerName = m_stageInfo->location;

  //Remove the trailing '/' in containerName
  containerName.pop_back();

  std::string blobName = fileMetadata->destFileName;

  //metadata azure uses.
  std::vector<std::pair<std::string, std::string>> userMetadata;
  addUserMetadata(&userMetadata, fileMetadata);
  //Calculate the length of the stream.
  unsigned int len = (unsigned int) (fileMetadata->encryptionMetadata.cipherStreamSize > 0) ? fileMetadata->encryptionMetadata.cipherStreamSize: fileMetadata->srcFileToUploadSize ;

  //Azure does not provide to SHA256 or MD5 or checksum check of a file to check if it already exists.
  bool exists = m_blobclient->blob_exists(containerName, blobName);
  if(exists)
  {
      return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
  }
  m_blobclient->upload_block_blob_from_stream(containerName, blobName, *dataStream, userMetadata, len);
  if (errno != 0)
      return RemoteStorageRequestOutcome::FAILED;

  return RemoteStorageRequestOutcome::SUCCESS;
}

void Snowflake::Client::SnowflakeAzureClient::uploadParts(MultiUploadCtx_a * uploadCtx)
{

}

RemoteStorageRequestOutcome SnowflakeAzureClient::doMultiPartUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start multi part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());

}

std::string buildEncryptionMetadataJSON(std::string iv64, std::string key64)
{
  char buf[512];
  sprintf(buf,"{\"EncryptionMode\":\"FullBlob\",\"WrappedContentKey\":{\"KeyId\":\"symmKey1\",\"EncryptedKey\":\"%s\",\"Algorithm\":\"AES_CBC_256\"},\"EncryptionAgent\":{\"Protocol\":\"1.0\",\"EncryptionAlgorithm\":\"AES_CBC_256\"},\"ContentEncryptionIV\":\"%s\", \"KeyWrappingMetadata\":{\"EncryptionLibrary\":\"Java 5.3.0\"}}", key64.c_str(), iv64.c_str());

  return std::string(buf);
}

void SnowflakeAzureClient::addUserMetadata(std::vector<std::pair<std::string, std::string>> *userMetadata, FileMetadata *fileMetadata)
{

  userMetadata->push_back(std::make_pair("matdesc", fileMetadata->encryptionMetadata.matDesc));

  char ivEncoded[64];
  bzero((void*)ivEncoded, 64);  //Base64::encode does not set the '\0' at the end of the string. (And this is the cause of failed decode on the server side). 
  Snowflake::Client::Util::Base64::encode(
          fileMetadata->encryptionMetadata.iv.data,
          Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES),
          ivEncoded);

  size_t ivEncodeSize = Snowflake::Client::Util::Base64::encodedLength(
          Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES));

  userMetadata->push_back(std::make_pair("encryptiondata", buildEncryptionMetadataJSON(ivEncoded, fileMetadata->encryptionMetadata.enKekEncoded) ));

}

RemoteStorageRequestOutcome SnowflakeAzureClient::download(
  FileMetadata *fileMetadata,
  std::basic_iostream<char>* dataStream)
{
  if (fileMetadata->srcFileSize > DATA_SIZE_THRESHOLD)
    return doMultiPartDownload(fileMetadata, dataStream);
  else
    return doSingleDownload(fileMetadata, dataStream);
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doMultiPartDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  CXX_LOG_DEBUG("Start multi part download for file %s, parallel: %d",
               fileMetadata->srcFileName.c_str(), m_parallel);

  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doSingleDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  CXX_LOG_DEBUG("Start single part download for file %s",
               fileMetadata->srcFileName.c_str());
  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::GetRemoteFileMetadata(
  std::string *filePathFull, FileMetadata *fileMetadata)
{
  return RemoteStorageRequestOutcome::SUCCESS;

}

}
}

#endif 
