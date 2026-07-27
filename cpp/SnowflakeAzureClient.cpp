#include "snowflake/SF_CRTFunctionSafe.h"
#include "SnowflakeAzureClient.hpp"
#include <azure/core/http/curl_transport.hpp>
#include "FileTransferAgent.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/client.h"
#include "util/Base64.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include "snowflake/Proxy.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include "logger/SFAwsLogger.hpp"
#include "logger/SFLogger.hpp"
#include "SnowflakeS3Client.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

#define CONTENT_TYPE_OCTET_STREAM "application/octet-stream"

using namespace Azure::Storage;
using namespace Azure::Storage::Blobs;

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
      CXX_LOG_TRACE("CA bundle file from TransferConfig *%s*", caBundleFile);
  }
  else if( caBundleFile[0] == 0 ) {
      SF_STATUS status = snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, caBundleFile, sizeof(caBundleFile));
      if (status == SF_STATUS_ERROR_BUFFER_TOO_SMALL) {
          throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
              "CA bundle file path too long.");
      }
      CXX_LOG_TRACE("CA bundle file from SF_GLOBAL_CA_BUNDLE_FILE *%s*", caBundleFile);
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
          CXX_LOG_TRACE("CA bundle file from SNOWFLAKE_TEST_CA_BUNDLE_FILE *%s*", caBundleFile);
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
  Util::Proxy * proxy;
  if (transferConfig && transferConfig->proxy) {
    proxy = transferConfig->proxy;
  }
  else {
    proxy = statement->get_proxy();
  }

  try {
    Azure::Storage::Blobs::BlobClientOptions options;
    options.Retry.MaxRetries = m_maxRetries;
    Azure::Core::Http::CurlTransportOptions curl_options;
    curl_options.CAInfo = caBundleFile;

    if (proxy) {
      curl_options.Proxy = proxy->getHost().empty() ?
          "" : proxy->getHost() + ":" + std::to_string(proxy->getPort());
      curl_options.NoProxy = proxy->getNoProxy();
      if (!proxy->getUser().empty())
      {
        curl_options.ProxyUsername = proxy->getUser();
      }
      if (!proxy->getPwd().empty())
      {
        curl_options.ProxyPassword = proxy->getPwd();
      }
    }

    options.Transport.Transport = std::make_shared<Azure::Core::Http::CurlTransport>(curl_options);
    m_blobServiceClient = std::make_shared<BlobServiceClient>(endpoint + sas_key, options);
  }
  catch (const std::exception& e) {
    CXX_LOG_ERROR("Failed to create Azure Service Client: %s", e.what());
    throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
        "Failed to create Azure Service Client: %s", e.what());
  }
  catch (...) {
    CXX_LOG_ERROR("Failed to create Azure Service Client: Unknown exception");
    throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
        "Failed to create Azure Service Client: Unknown exception");
  }

  //Ensure the stage location ended with /
  if ((!m_stageInfo->location.empty()) && (m_stageInfo->location.back() != '/'))
  {
    m_stageInfo->location.push_back('/');
  }

  CXX_LOG_TRACE("Successfully created Azure client. End of constructor.");
}

SnowflakeAzureClient::~SnowflakeAzureClient()
{
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

  //Calculate the length of the stream.
  int64_t len = (int64_t) ((fileMetadata->encryptionMetadata.cipherStreamSize > 0) ? fileMetadata->encryptionMetadata.cipherStreamSize: fileMetadata->srcFileToUploadSize) ;

  try {
    auto containerClient = m_blobServiceClient->GetBlobContainerClient(containerName);
    auto blobClient = containerClient.GetBlockBlobClient(blobName);
    //metadata azure uses.
    UploadBlockBlobOptions uploadOptions;
    addUserMetadata(uploadOptions.Metadata, fileMetadata);
    //Azure does not provide to SHA256 or MD5 or checksum check of a file to check if it already exists.
    //Do not check if file exists if overwrite is specified.
    if(! fileMetadata->overWrite ) {
      try {
        blobClient.GetProperties();
        // GetProperties() succeeded means the file exists.
        CXX_LOG_DEBUG("File already exists skipping the file upload %s",
          fileMetadata->srcFileToUpload.c_str());
        return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
      }
      catch (const Azure::Storage::StorageException& e)
      {
        // NotFound is expected otherwise throw error
        if (e.StatusCode != Azure::Core::Http::HttpStatusCode::NotFound) {
          CXX_LOG_ERROR("Failed to check file existence: %s", e.what());
          throw;
        }
      }
    }

    AzureStreamAdapter upwardStream(*dataStream, len);
    blobClient.Upload(upwardStream, uploadOptions);
  }
  catch (const std::exception& e) {
    CXX_LOG_ERROR("%s single part upload failed: %s",
        fileMetadata->srcFileToUpload.c_str(), e.what());
    return RemoteStorageRequestOutcome::FAILED;
  }
  catch (...) {
    CXX_LOG_ERROR("%s single part upload failed:: Unknown exception",
        fileMetadata->srcFileToUpload.c_str());
    return RemoteStorageRequestOutcome::FAILED;
  }

  CXX_LOG_DEBUG("%s single part upload successful.",
                fileMetadata->srcFileToUpload.c_str());
  return RemoteStorageRequestOutcome::SUCCESS;
}

void Snowflake::Client::SnowflakeAzureClient::uploadParts(BlockBlobClient* blobClient,
  MultiUploadCtx_a* uploadCtx)
{
  try
  {
    Azure::Core::IO::MemoryBodyStream memoryStream((uint8*)(uploadCtx->buf->getDataBuffer()),
                                                   uploadCtx->buf->getSize());
    blobClient->StageBlock(uploadCtx->m_uploadId, memoryStream);
    CXX_LOG_DEBUG("Upload parts request succeed. part number %d",
                  uploadCtx->m_partNumber);
    uploadCtx->m_outcome = RemoteStorageRequestOutcome::SUCCESS;
  }
  catch (const std::exception& e) {
    CXX_LOG_ERROR("Upload parts request Failed. part number %d, error: %s",
                  uploadCtx->m_partNumber, e.what());
    uploadCtx->m_outcome = RemoteStorageRequestOutcome::FAILED;
  }
  catch (...) {
    CXX_LOG_ERROR("Upload parts request Failed. part number %d, error: Unknown exception",
                  uploadCtx->m_partNumber);
    uploadCtx->m_outcome = RemoteStorageRequestOutcome::FAILED;
  }
}

void SnowflakeAzureClient::setMaxRetries(unsigned int maxRetries)
{
    m_maxRetries = maxRetries;
}


RemoteStorageRequestOutcome SnowflakeAzureClient::doMultiPartUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start multi part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());

  if (m_threadPool == nullptr)
  {
      m_threadPool = new Util::ThreadPool(m_parallel);
  }

  std::string containerName = m_stageInfo->location;
  //Remove the trailing '/' in containerName
  containerName.pop_back();

  std::string blobName = fileMetadata->destFileName;

  //Calculate the length of the stream.
  size_t len = (size_t)((fileMetadata->encryptionMetadata.cipherStreamSize > 0) ? fileMetadata->encryptionMetadata.cipherStreamSize : fileMetadata->srcFileToUploadSize);

  try {
    auto containerClient = m_blobServiceClient->GetBlobContainerClient(containerName);
    auto blobClient = containerClient.GetBlockBlobClient(blobName);
    //metadata azure uses.
    CommitBlockListOptions commitOptions;
    addUserMetadata(commitOptions.Metadata, fileMetadata);
    //Do not check if file exists if overwrite is specified.
    if(! fileMetadata->overWrite ) {
      try {
        blobClient.GetProperties();
        // GetProperties() succeeded means the file exists.
        CXX_LOG_DEBUG("File already exists skipping the file upload %s",
          fileMetadata->srcFileToUpload.c_str());
        return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
      }
      catch (const Azure::Storage::StorageException& e)
      {
        // NotFound is expected otherwise throw error
        if (e.StatusCode != Azure::Core::Http::HttpStatusCode::NotFound) {
          CXX_LOG_ERROR("Failed to check file existence: %s", e.what());
          throw;
        }
      }
    }

    Util::StreamSplitter splitter(dataStream, m_parallel, m_uploadThreshold);
    unsigned int totalParts = splitter.getTotalParts(len);
    CXX_LOG_DEBUG("Total file size: %d, split into %d parts.", len, totalParts);

    std::vector<std::string> blockIds(totalParts);
    std::vector<MultiUploadCtx_a> uploadParts;
    uploadParts.reserve(totalParts);

    for (unsigned int i = 0; i < totalParts; i++)
    {
      std::ostringstream ss;
      ss << std::setw(6) << std::setfill('0') << i;
      std::string idStr = ss.str();
      std::vector<char> vec(idStr.begin(), idStr.end());
      blockIds[i] = Util::Base64::encodePadding(vec);
      uploadParts.emplace_back(i, blockIds[i]);
    }

    for (unsigned int i = 0; i < totalParts; i++)
    {
      m_threadPool->AddJob([&splitter, this, &blobClient, &blockIds, &uploadParts]()->void
                           {
                             int tid = m_threadPool->GetThreadIdx();
                             int partId;
                             Util::ByteArrayStreamBuf * buf = splitter.FillAndGetBuf(tid, partId);
                             uploadParts[partId].buf = buf;
                             this->uploadParts(&blobClient, &uploadParts[partId]);
                           });
    }

    m_threadPool->WaitAll();

    // check result for each part
    for (unsigned int i=0; i< totalParts; i++)
    {
      if (uploadParts[i].m_outcome != RemoteStorageRequestOutcome::SUCCESS)
      {
        return uploadParts[i].m_outcome;
      }
    }

    blobClient.CommitBlockList(blockIds, commitOptions);
  }
  catch (const std::exception& e) {
    CXX_LOG_ERROR("%s file upload failed: %s",
        fileMetadata->srcFileToUpload.c_str(), e.what());
    return RemoteStorageRequestOutcome::FAILED;
  }
  catch (...) {
    CXX_LOG_ERROR("%s file upload failed:: Unknown exception",
        fileMetadata->srcFileToUpload.c_str());
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

void SnowflakeAzureClient::addUserMetadata(Azure::Storage::Metadata& userMetadata, FileMetadata *fileMetadata)
{

  userMetadata["matdesc"] = fileMetadata->encryptionMetadata.matDesc;

  char ivEncoded[64];
  memset((void*)ivEncoded, 0, 64);  //Base64::encode does not set the '\0' at the end of the string. (And this is the cause of failed decode on the server side). 
  Snowflake::Client::Util::Base64::encode(
          fileMetadata->encryptionMetadata.iv.data,
          Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES),
          ivEncoded);

  size_t ivEncodeSize = Snowflake::Client::Util::Base64::encodedLength(
          Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES));

  userMetadata["encryptiondata"] = buildEncryptionMetadataJSON(ivEncoded, fileMetadata->encryptionMetadata.enKekEncoded);

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
  try {
    auto containerClient = m_blobServiceClient->GetBlobContainerClient(cont);
    auto blobClient = containerClient.GetBlockBlobClient(blob);
    auto resp = blobClient.GetProperties();
    auto blobprop = resp.Value;
    fileMetadata->srcFileSize = (size_t)blobprop.BlobSize;
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

        try {
          DownloadBlobOptions options;
          options.Range = Azure::Core::Http::HttpRange{ ctx.startbyte, partSize };
          auto response = blobClient.Download(options);
          partSize = (int)(response.Value.BodyStream->ReadToCount((uint8*)buf->getDataBuffer(),
                                                                partSize));
          buf->updateSize(partSize);
          CXX_LOG_DEBUG("Download part %d succeed, download size: %d",
                      ctx.m_partNumber, partSize);
          appender.WritePartToOutputStream(m_threadPool->GetThreadIdx(),
                                           ctx.m_partNumber);
          ctx.m_outcome = RemoteStorageRequestOutcome::SUCCESS;
        }
        catch (const std::exception& e) {
          CXX_LOG_ERROR("Download part %d FAILED, download size: %d, error: %s",
                        ctx.m_partNumber, partSize, e.what());
          ctx.m_outcome = RemoteStorageRequestOutcome::FAILED;
        }
        catch (...) {
          CXX_LOG_ERROR("Download part %d FAILED, download size: %d, error: Unknown exception",
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
  }
  catch (const std::exception& e) {
    CXX_LOG_ERROR("%s file download failed: %s",
                  fileMetadata->srcFileName.c_str(), e.what());
    return RemoteStorageRequestOutcome::FAILED;
  }
  catch (...) {
    CXX_LOG_ERROR("%s file download failed:: Unknown exception",
                  fileMetadata->srcFileName.c_str());
    return RemoteStorageRequestOutcome::FAILED;
  }

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
  try {
    auto containerClient = m_blobServiceClient->GetBlobContainerClient(cont);
    auto blobClient = containerClient.GetBlockBlobClient(blob);
    auto response = blobClient.Download();
    if (!response.Value.BodyStream)
    {
      CXX_LOG_ERROR("%s file donwload failed:: BodyStream is NULL",
                    fileMetadata->srcFileName.c_str());
      return RemoteStorageRequestOutcome::FAILED;
    }

    auto& bodySteam = *response.Value.BodyStream;
    constexpr size_t bufferSize = 4096;
    std::vector<uint8_t> buffer(bufferSize);
    while (true)
    {
      size_t bytesRead = bodySteam.Read(buffer.data(), buffer.size());
      if (bytesRead == 0)
      {
        break; // End of stream
      }
      dataStream->write(reinterpret_cast<char*>(buffer.data()),
                        static_cast<std::streamsize>(bytesRead));
    }
    dataStream->flush();
  }
  catch (const std::exception& e) {
    CXX_LOG_ERROR("%s file download failed: %s",
                  fileMetadata->srcFileName.c_str(), e.what());
    return RemoteStorageRequestOutcome::FAILED;
  }
  catch (...) {
    CXX_LOG_ERROR("%s file download failed:: Unknown exception",
                  fileMetadata->srcFileName.c_str());
    return RemoteStorageRequestOutcome::FAILED;
  }

  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::GetRemoteFileMetadata(
  std::string *filePathFull, FileMetadata *fileMetadata)
{
  unsigned long dirSep = (unsigned long)filePathFull->find_last_of('/');
  std::string blob = filePathFull->substr(dirSep + 1);
  std::string cont = filePathFull->substr(0,dirSep);
  try {
    auto containerClient = m_blobServiceClient->GetBlobContainerClient(cont);
    auto blobClient = containerClient.GetBlockBlobClient(blob);
    auto resp = blobClient.GetProperties();
    auto blobProperty = resp.Value;
    std::string encHdr = blobProperty.Metadata["encryptiondata"];
    fileMetadata->srcFileSize = (size_t)blobProperty.BlobSize;
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
      if (Util::Base64::decode(iv.c_str(), iv.size(),
            fileMetadata->encryptionMetadata.iv.data,
            sizeof(fileMetadata->encryptionMetadata.iv.data))
          == static_cast<size_t>(-1L))
      {
        CXX_LOG_ERROR("Invalid or oversized IV in blob metadata for %s; "
                      "rejecting download.", blob.c_str());
        return RemoteStorageRequestOutcome::FAILED;
      }
    }

    fileMetadata->encryptionMetadata.cipherStreamSize = blobProperty.BlobSize;
    fileMetadata->srcFileSize = (size_t)blobProperty.BlobSize;
  }
  catch (const Azure::Storage::StorageException& e)
  {
    // NotFound is expected otherwise throw error
    if (e.StatusCode == Azure::Core::Http::HttpStatusCode::NotFound)
    {
      CXX_LOG_DEBUG("File does not exist: %s", filePathFull->c_str());
      return RemoteStorageRequestOutcome::FAILED;
    }
    else
    {
      CXX_LOG_ERROR("Failed to get file %s metadata: %s",
                    filePathFull->c_str(), e.what());
      return RemoteStorageRequestOutcome::FAILED;
    }
  }
  catch (...)
  {
    CXX_LOG_ERROR("Failed to get file %s metadata: Unknown exception",
                  filePathFull->c_str());
    return RemoteStorageRequestOutcome::FAILED;
  }

  return RemoteStorageRequestOutcome::SUCCESS;
}

}
}
