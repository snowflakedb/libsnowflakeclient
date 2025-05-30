#include "snowflake/SF_CRTFunctionSafe.h"
#include "SnowflakeAzureClient.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/client.h"
#include "util/Base64.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include "snowflake/Proxy.hpp"
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


SnowflakeAzureClient::SnowflakeAzureClient(StageInfo *stageInfo,
                                           unsigned int parallel,
                                           size_t uploadThreshold,
                                           TransferConfig *transferConfig,
                                           IStatementPutGet* statement) :
  m_stageInfo(stageInfo),
  m_threadPool(nullptr),
  m_uploadThreshold(uploadThreshold),
  m_parallel(std::min(parallel, std::thread::hardware_concurrency()))
{
  const std::string azuresaskey("AZURE_SAS_KEY");
  char caBundleFile[MAX_PATH] = {0};
  if(transferConfig && transferConfig->caBundleFile) {
      size_t len = strlen(transferConfig->caBundleFile);
      if ( len > MAX_PATH - 1) {
        throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
            "CA bundle file path too long.");
      }
      if( ! sf_strcpy(caBundleFile, (size_t)MAX_PATH, transferConfig->caBundleFile) ) {
        caBundleFile[0] = 0;
      }
      CXX_LOG_TRACE("ca bundle file from TransferConfig *%s*", caBundleFile);
  }
  else if( caBundleFile[0] == 0 ) {
      SF_STATUS status = snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, caBundleFile, sizeof(caBundleFile));
      if (status == SF_STATUS_ERROR_BUFFER_TOO_SMALL) {
          throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
              "CA bundle file path too long.");
      }
      CXX_LOG_TRACE("ca bundle file from SF_GLOBAL_CA_BUNDLE_FILE *%s*", caBundleFile);
  }
  if( caBundleFile[0] == 0 ) {
      char capath_buf[MAX_PATH + 2];
      char* capath = sf_getenv_s("SNOWFLAKE_TEST_CA_BUNDLE_FILE", capath_buf, sizeof(capath_buf));
      if (capath) {
          if (strlen(capath) > MAX_PATH - 1) {
              throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
                  "CA bundle file path too long.");
          }
          if (!sf_strcpy(caBundleFile, (size_t)MAX_PATH, capath)) {
              caBundleFile[0] = 0;
          }
          CXX_LOG_TRACE("ca bundle file from SNOWFLAKE_TEST_CA_BUNDLE_FILE *%s*", caBundleFile);
      }
  }
  if(caBundleFile[0] == 0) {
    CXX_LOG_ERROR("CA bundle file is empty.");
    throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
                                     "CA bundle file is empty.");
  }

  std::string account_name = m_stageInfo->storageAccount;
  std::string sas_key = m_stageInfo->credentials[azuresaskey];
  std::string endpoint = account_name + "." + m_stageInfo->endPoint;
  std::shared_ptr<azure::storage_lite::storage_credential>  cred = std::make_shared<azure::storage_lite::shared_access_signature_credential>(sas_key);
  std::shared_ptr<azure::storage_lite::storage_account> account = std::make_shared<azure::storage_lite::storage_account>(account_name, cred, true, endpoint);
  std::shared_ptr<azure::storage_lite::blob_client> bc;

  Util::Proxy * proxy;
  if (transferConfig && transferConfig->proxy) {
    proxy = transferConfig->proxy;
  }
  else {
    proxy = statement->get_proxy();
  }
  if (proxy) {
    bc = std::make_shared<azure::storage_lite::blob_client>(
                account, m_parallel, caBundleFile,
                proxy->getHost(),
                proxy->getPort(),
                proxy->getUser(),
                proxy->getPwd(),
                proxy->getNoProxy());
  }
  else
  {
     bc = std::make_shared<azure::storage_lite::blob_client>(account, m_parallel, caBundleFile);
  }
  m_blobclient= new azure::storage_lite::blob_client_wrapper(bc);
  //Ensure the stage location ended with /
  if ((!m_stageInfo->location.empty()) && (m_stageInfo->location.back() != '/'))
  {
    m_stageInfo->location.push_back('/');
  }

  CXX_LOG_TRACE("Successfully created Azure client. End of constructor.");
}

SnowflakeAzureClient::~SnowflakeAzureClient()
{
    delete m_blobclient;
    if (m_threadPool != nullptr)
    {
        delete m_threadPool;
    }
}

RemoteStorageRequestOutcome SnowflakeAzureClient::upload(FileMetadata *fileMetadata,
                                          std::basic_iostream<char> *dataStream)
{
    if(fileMetadata->encryptionMetadata.cipherStreamSize <= m_uploadThreshold)
    {
        return doSingleUpload(fileMetadata, dataStream);
    }
    return doMultiPartUpload(fileMetadata, dataStream);
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
  size_t len = (size_t) ((fileMetadata->encryptionMetadata.cipherStreamSize > 0) ? fileMetadata->encryptionMetadata.cipherStreamSize: fileMetadata->srcFileToUploadSize) ;

  //Azure does not provide to SHA256 or MD5 or checksum check of a file to check if it already exists.
  //Do not check if file exists if overwrite is specified.
  if(! fileMetadata->overWrite ) {
      bool exists = m_blobclient->blob_exists(containerName, blobName);
      if (exists) {
        CXX_LOG_DEBUG("File already exists skipping the file upload %s",
                      fileMetadata->srcFileToUpload.c_str());
          return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
      }
  }
  m_blobclient->upload_block_blob_from_stream(containerName, blobName, *dataStream, userMetadata, len);
  if (errno != 0)
  {
    CXX_LOG_ERROR("%s single part upload failed, errno = %d",
                  fileMetadata->srcFileToUpload.c_str(), errno);
      return RemoteStorageRequestOutcome::FAILED;
  }

  CXX_LOG_DEBUG("%s single part upload successful.",
                fileMetadata->srcFileToUpload.c_str());
  return RemoteStorageRequestOutcome::SUCCESS;
}

void Snowflake::Client::SnowflakeAzureClient::uploadParts(MultiUploadCtx_a * uploadCtx)
{
	return;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doMultiPartUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start multi part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());
  std::string containerName = m_stageInfo->location;

    //Remove the trailing '/' in containerName
    containerName.pop_back();

    std::string blobName = fileMetadata->destFileName;

    //metadata azure uses.
    std::vector<std::pair<std::string, std::string>> userMetadata;
    addUserMetadata(&userMetadata, fileMetadata);
    //Calculate the length of the stream.
    size_t len = (size_t) ((fileMetadata->encryptionMetadata.cipherStreamSize > 0) ? fileMetadata->encryptionMetadata.cipherStreamSize: fileMetadata->srcFileToUploadSize) ;
    if(! fileMetadata->overWrite ) {
        //Azure does not provide to SHA256 or MD5 or checksum check of a file to check if it already exists.
        bool exists = m_blobclient->blob_exists(containerName, blobName);
        if (exists) {
          CXX_LOG_DEBUG("File already exists skipping the file upload %s",
                        fileMetadata->srcFileToUpload.c_str());
            return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
        }
    }
    m_blobclient->multipart_upload_block_blob_from_stream(containerName, blobName, *dataStream, userMetadata, len);
    if (errno != 0)
    {
      CXX_LOG_ERROR("%s file upload failed, errno = %d.", fileMetadata->srcFileToUpload.c_str(), errno);
        return RemoteStorageRequestOutcome::FAILED;
    }
    CXX_LOG_DEBUG("%s file upload success.", fileMetadata->srcFileToUpload.c_str());
    return RemoteStorageRequestOutcome::SUCCESS;
}

std::string buildEncryptionMetadataJSON(std::string iv64, std::string enkek64)
{
  char buf[512];
  sf_sprintf(buf, sizeof(buf), "{\"EncryptionMode\":\"FullBlob\",\"WrappedContentKey\":{\"KeyId\":\"symmKey1\",\"EncryptedKey\":\"%s\",\"Algorithm\":\"AES_CBC_256\"},\"EncryptionAgent\":{\"Protocol\":\"1.0\",\"EncryptionAlgorithm\":\"AES_CBC_256\"},\"ContentEncryptionIV\":\"%s\", \"KeyWrappingMetadata\":{\"EncryptionLibrary\":\"Java 5.3.0\"}}", enkek64.c_str(), iv64.c_str());

  return std::string(buf);
}

void SnowflakeAzureClient::addUserMetadata(std::vector<std::pair<std::string, std::string>> *userMetadata, FileMetadata *fileMetadata)
{

  userMetadata->push_back(std::make_pair("matdesc", fileMetadata->encryptionMetadata.matDesc));

  char ivEncoded[64];
  memset((void*)ivEncoded, 0, 64);  //Base64::encode does not set the '\0' at the end of the string. (And this is the cause of failed decode on the server side). 
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
  if (fileMetadata->isLarge)
    return doMultiPartDownload(fileMetadata, dataStream);
  else
    return doSingleDownload(fileMetadata, dataStream);
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doMultiPartDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream) {

    CXX_LOG_DEBUG("Start multi part download for file %s, parallel: %d",
                  fileMetadata->srcFileName.c_str(), m_parallel);
    unsigned long dirSep = (unsigned long)fileMetadata->srcFileName.find_last_of('/');
    std::string blob = fileMetadata->srcFileName.substr(dirSep + 1);
    std::string cont = fileMetadata->srcFileName.substr(0,dirSep);

    if (m_threadPool == nullptr)
    {
        m_threadPool = new Util::ThreadPool(m_parallel);
    }

    //To fetch size of file.
    auto blobprop = m_blobclient->get_blob_property(cont, blob);
    std::string origEtag = blobprop.etag ;
    fileMetadata->srcFileSize = (size_t)blobprop.size;
    unsigned int partNum = (unsigned int)(fileMetadata->srcFileSize / DOWNLOAD_DATA_SIZE_THRESHOLD) + 1;

    Util::StreamAppender appender(dataStream, partNum, m_parallel, DOWNLOAD_DATA_SIZE_THRESHOLD);
    std::vector<MultiDownloadCtx_a> downloadParts;
    for (unsigned int i = 0; i < partNum; i++)
    {
        downloadParts.emplace_back();
        downloadParts.back().m_partNumber = i;
        downloadParts.back().startbyte = i * DOWNLOAD_DATA_SIZE_THRESHOLD ;
    }

    for (int i = 0; i < downloadParts.size(); i++)
    {
        MultiDownloadCtx_a &ctx = downloadParts[i];

        m_threadPool->AddJob([&]()-> void {

            int partSize = ctx.m_partNumber == partNum - 1 ?
                           (int)(fileMetadata->srcFileSize -
                                 (size_t)ctx.m_partNumber * DOWNLOAD_DATA_SIZE_THRESHOLD)
                                                           : DOWNLOAD_DATA_SIZE_THRESHOLD;
            Util::ByteArrayStreamBuf * buf = appender.GetBuffer(
                    m_threadPool->GetThreadIdx());

            CXX_LOG_DEBUG("Start downloading part %d, Start Byte: %d, part size: %d",
                          ctx.m_partNumber, ctx.startbyte,
                          partSize);
            std::shared_ptr <std::stringstream> chunkbuff = std::make_shared<std::stringstream>();

            m_blobclient->get_chunk(cont, blob, ctx.startbyte, partSize, origEtag, chunkbuff );

            if ( ! errno)
            {
                chunkbuff->read(buf->getDataBuffer(), chunkbuff->str().size());
                buf->updateSize(partSize);
                CXX_LOG_DEBUG("Download part %d succeed, download size: %d",
                              ctx.m_partNumber, partSize);
                ctx.m_outcome = RemoteStorageRequestOutcome::SUCCESS;
                appender.WritePartToOutputStream(m_threadPool->GetThreadIdx(),
                                                 ctx.m_partNumber);
                chunkbuff->str(std::string());
            }
            else
            {
                CXX_LOG_DEBUG("Download part %d FAILED, download size: %d",
                              ctx.m_partNumber, partSize);
                ctx.m_outcome = RemoteStorageRequestOutcome::FAILED;
            }
        });
    }

    m_threadPool->WaitAll();

    for (unsigned int i = 0; i < partNum; i++)
    {
        if (downloadParts[i].m_outcome != RemoteStorageRequestOutcome::SUCCESS)
        {
            return downloadParts[i].m_outcome;
        }
    }
    dataStream->flush();

  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doSingleDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  CXX_LOG_DEBUG("Start single part download for file %s",
               fileMetadata->srcFileName.c_str());
  unsigned long dirSep = (unsigned long)fileMetadata->srcFileName.find_last_of('/');
  std::string blob = fileMetadata->srcFileName.substr(dirSep + 1);
  std::string cont = fileMetadata->srcFileName.substr(0,dirSep);
  unsigned long long offset=0;
  m_blobclient->download_blob_to_stream(cont, blob, offset, fileMetadata->srcFileSize, *dataStream);
  dataStream->flush();
  if(errno == 0)
    return RemoteStorageRequestOutcome::SUCCESS;

  return RemoteStorageRequestOutcome ::FAILED;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::GetRemoteFileMetadata(
  std::string *filePathFull, FileMetadata *fileMetadata)
{
    unsigned long dirSep = (unsigned long)filePathFull->find_last_of('/');
    std::string blob = filePathFull->substr(dirSep + 1);
    std::string cont = filePathFull->substr(0,dirSep);
    auto blobProperty = m_blobclient->get_blob_property(cont, blob  );
    if(blobProperty.valid()) {
        std::string encHdr = blobProperty.metadata[0].second;
        fileMetadata->srcFileSize = (size_t)blobProperty.size;
        encHdr.erase(remove(encHdr.begin(), encHdr.end(), ' '), encHdr.end());  //Remove spaces from the string.

        std::size_t pos1 = encHdr.find("EncryptedKey")  + strlen("EncryptedKey") + 3;
        std::size_t pos2 = encHdr.find("\",\"Algorithm\"");
        if ((std::string::npos != pos1) && (std::string::npos != pos2) && (pos2 >= pos1))
        {
          fileMetadata->encryptionMetadata.enKekEncoded = encHdr.substr(pos1, pos2 - pos1);
        }

        pos1 = encHdr.find("ContentEncryptionIV")  + strlen("ContentEncryptionIV") + 3;
        pos2 = encHdr.find("\",\"KeyWrappingMetadata\"");
        std::string iv("");
        if ((std::string::npos != pos1) && (std::string::npos != pos2) && (pos2 >= pos1))
        {
          iv = encHdr.substr(pos1, pos2 - pos1);
          Util::Base64::decode(iv.c_str(), iv.size(), fileMetadata->encryptionMetadata.iv.data);
        }

        fileMetadata->encryptionMetadata.cipherStreamSize = blobProperty.size;
        fileMetadata->srcFileSize = (size_t)blobProperty.size;

        return RemoteStorageRequestOutcome::SUCCESS;
    }

    return RemoteStorageRequestOutcome::FAILED;

}


}
}
