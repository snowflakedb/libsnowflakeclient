/*
 * Copyright (c) 2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifdef __linux__

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
  char caBundleFile[512]={0};
  if(transferConfig && transferConfig->caBundleFile) {
      int len = strlen(transferConfig->caBundleFile);
      len = (len >=511)? 511: len;
      strncpy(caBundleFile, transferConfig->caBundleFile, len);
      caBundleFile[511]=0;
      CXX_LOG_TRACE("ca bundle file from TransferConfig *%s*", caBundleFile);
  }
  else{
      snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, caBundleFile);
      CXX_LOG_TRACE("ca bundle file from SF_GLOBAL_CA_BUNDLE_FILE *%s*", caBundleFile);
  }
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

std::string buildEncryptionMetadataJSON(std::string iv64, std::string enkek64)
{
  char buf[512];
  sprintf(buf,"{\"EncryptionMode\":\"FullBlob\",\"WrappedContentKey\":{\"KeyId\":\"symmKey1\",\"EncryptedKey\":\"%s\",\"Algorithm\":\"AES_CBC_256\"},\"EncryptionAgent\":{\"Protocol\":\"1.0\",\"EncryptionAlgorithm\":\"AES_CBC_256\"},\"ContentEncryptionIV\":\"%s\", \"KeyWrappingMetadata\":{\"EncryptionLibrary\":\"Java 5.3.0\"}}", enkek64.c_str(), iv64.c_str());

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
    return doSingleDownload(fileMetadata, dataStream);

    // TODO:: Support Multi part download for Azure.

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
  unsigned long dirSep = fileMetadata->srcFileName.find_last_of('/');
  std::string blob = fileMetadata->srcFileName.substr(dirSep + 1);
  std::string cont = fileMetadata->srcFileName.substr(0,dirSep);
  unsigned long long offset=0;
  m_blobclient->download_blob_to_stream(cont, blob, offset, fileMetadata->destFileSize, *dataStream);
  if(errno == 0)
    return RemoteStorageRequestOutcome::SUCCESS;

  return RemoteStorageRequestOutcome ::FAILED;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::GetRemoteFileMetadata(
  std::string *filePathFull, FileMetadata *fileMetadata)
{
    unsigned long dirSep = filePathFull->find_last_of('/');
    std::string blob = filePathFull->substr(dirSep + 1);
    std::string cont = filePathFull->substr(0,dirSep);
    auto blobProperty = m_blobclient->get_blob_property(cont, blob  );
    if(blobProperty.valid()) {
        std::string encry = blobProperty.metadata[0].second;
        std::string matdesc = blobProperty.metadata[1].second;

        encry.erase(remove(encry.begin(), encry.end(), ' '), encry.end());  //Remove spaces from the string.

        //sscanf(encry.c_str()," \"{\"EncryptionMode\":\"FullBlob\",\"WrappedContentKey\":{\"KeyId\":\"symmKey1\",\"EncryptedKey\":\"%[^\"]\",\"Algorithm\":\"AES_CBC_256\"},\"EncryptionAgent\":{\"Protocol\":\"1.0\",\"EncryptionAlgorithm\":\"AES_CBC_256\"},\"ContentEncryptionIV\":\"%[^\"]\",\"KeyWrappingMetadata\":{\"EncryptionLibrary\":\"Java 5.3.0\"}}\"", enkek ,iv);

        //TODO: There is a better way of doing this but might complicate things. 
        //An assumption made is the key value pair order is not going to change. 
        //Will optimize 
        std::size_t pos1 = encry.find("EncryptedKey")  + strlen("EncryptedKey") + 3;
        std::size_t pos2 = encry.find("\",\"Algorithm\"");
        fileMetadata->encryptionMetadata.enKekEncoded = encry.substr(pos1, pos2-pos1);

        pos1 = encry.find("ContentEncryptionIV")  + strlen("ContentEncryptionIV") + 3;
        pos2 = encry.find("\",\"KeyWrappingMetadata\"");
        std::string iv = encry.substr(pos1, pos2-pos1);

        Util::Base64::decode(iv.c_str(), iv.size(), fileMetadata->encryptionMetadata.iv.data);

        fileMetadata->encryptionMetadata.cipherStreamSize = blobProperty.size;
        fileMetadata->srcFileSize = blobProperty.size;

        return RemoteStorageRequestOutcome::SUCCESS;
    }

    return RemoteStorageRequestOutcome::FAILED;

}

}
}

#endif 
