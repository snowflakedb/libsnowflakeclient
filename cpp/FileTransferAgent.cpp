/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <memory>
#include <fstream>
#include <iostream>
#include <vector>
#include "FileTransferAgent.hpp"
#include "snowflake/SnowflakeTransferException.hpp"
#include "snowflake/IStatementPutGet.hpp"
#include "util/Base64.hpp"
#include "SnowflakeS3Client.hpp"
#include "StorageClientFactory.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include "crypto/Cryptor.hpp"
#include "util/CompressionUtil.hpp"
#include "util/ThreadPool.hpp"
#include "EncryptionProvider.hpp"
#include "logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "snowflake/Simba_CRTFunctionSafe.h"
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
using ::std::string;
using ::std::vector;
using ::Snowflake::Client::RemoteStorageRequestOutcome;

namespace
{
  const std::string FILE_PROTOCOL = "file://";

  void replaceStrAll(std::string& stringToReplace,
                  std::string const& oldValue,
                  std::string const& newValue)
  {
    size_t oldValueLen = oldValue.length();
    size_t newValueLen = newValue.length();
    if (0 == oldValueLen)
    {
      return;
    }

    size_t index = 0;
    while (true) {
      /* Locate the substring to replace. */
      index = stringToReplace.find(oldValue, index);
      if (index == std::string::npos) break;

      /* Make the replacement. */
      stringToReplace.replace(index, oldValueLen, newValue);

      /* Advance index forward so the next iteration doesn't pick it up as well. */
      index += newValueLen;
    }
  }
}

Snowflake::Client::FileTransferAgent::FileTransferAgent(
  IStatementPutGet *statement,
  TransferConfig *transferConfig) :
  m_stmtPutGet(statement),
  m_FileMetadataInitializer(m_smallFilesMeta, m_largeFilesMeta, statement),
  m_executionResults(nullptr),
  m_storageClient(nullptr),
  m_lastRefreshTokenSec(0),
  m_transferConfig(transferConfig),
  m_uploadStream(nullptr),
  m_uploadStreamSize(0),
  m_useDevUrand(false),
  m_maxPutRetries(5),
  m_putFastFail(false),
  m_maxGetRetries(5),
  m_getFastFail(false)
{
  _mutex_init(&m_parallelTokRenewMutex);
  _mutex_init(&m_parallelFailedMsgMutex);
}

Snowflake::Client::FileTransferAgent::~FileTransferAgent()
{
  reset();
  _mutex_term(&m_parallelTokRenewMutex);
  _mutex_term(&m_parallelFailedMsgMutex);
}

void Snowflake::Client::FileTransferAgent::reset()
{
  if (m_executionResults != nullptr)
  {
    delete m_executionResults;
    m_executionResults = nullptr;
  }

  if (m_storageClient != nullptr)
  {
    delete m_storageClient;
    m_storageClient = nullptr;
  }

  m_largeFilesMeta.clear();
  m_smallFilesMeta.clear();
  m_failedTransfers.clear();
}

Snowflake::Client::ITransferResult *
Snowflake::Client::FileTransferAgent::execute(string *command)
{
  reset();

  // first parse command
  if (!m_stmtPutGet->parsePutGetCommand(command, &response))
  {
    throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
      "Failed to parse response.");
  }
  CXX_LOG_INFO("Parse response succeed");

  // init storage client
  m_storageClient = StorageClientFactory::getClient(&response.stageInfo,
                                                    (unsigned int) response.parallel,
                                                    response.threshold,
                                                    m_transferConfig,
                                                    m_stmtPutGet);

  // init file metadata
  initFileMetadata(command);
  m_storageClient->setMaxRetries(m_maxPutRetries);

  switch (response.command)
  {
    case CommandType::UPLOAD: {
      auto putStart = std::chrono::steady_clock::now();
      upload(command);
      auto putEnd = std::chrono::steady_clock::now();
      auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(putEnd - putStart).count();
      CXX_LOG_DEBUG("Time took to upload %s: %ld milli seconds.", command->c_str(), totalTime);
      break;
    }
    case CommandType::DOWNLOAD:
      download(command);
      break;

    default:
      throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
        "Invalid command type.");
  }

  return m_executionResults;
}

void Snowflake::Client::FileTransferAgent::initFileMetadata(std::string *command)
{
  m_FileMetadataInitializer.setAutoCompress(response.autoCompress);
  m_FileMetadataInitializer.setSourceCompression(response.sourceCompression);
  m_FileMetadataInitializer.setEncryptionMaterials(&response.encryptionMaterials);
  m_FileMetadataInitializer.setRandomDev(m_useDevUrand);

  // Upload data from stream in memory
  if ((m_uploadStream) && (CommandType::UPLOAD == response.command))
  {
    // Only single file uploading supported, the data must be pre-compressed with gzip
    if ((response.srcLocations.size() != 1) || response.autoCompress ||
        sf_strncasecmp(response.sourceCompression, "gzip", sizeof("gzip")))
    {
      CXX_LOG_FATAL(CXX_LOG_NS, "Invalid stream uploading.");
      throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
        "Invalid stream uploading.");
    }

    FileMetadata fileMeta;
    fileMeta.srcFileName = response.srcLocations.at(0);
    fileMeta.srcFileSize = m_uploadStreamSize;
    fileMeta.destFileName = fileMeta.srcFileName;
    fileMeta.sourceCompression = &FileCompressionType::GZIP;
    fileMeta.requireCompress = false;
    fileMeta.targetCompression = &FileCompressionType::GZIP;
    m_largeFilesMeta.push_back(fileMeta);

    return;
  }

  vector<string> *sourceLocations = &response.srcLocations;
  for (size_t i = 0; i < sourceLocations->size(); i++)
  {
    switch (response.command)
    {
      case CommandType::UPLOAD:
        m_FileMetadataInitializer.populateSrcLocUploadMetadata(
            sourceLocations->at(i), response.threshold);
        break;
      case CommandType::DOWNLOAD:
        {
          std::string presignedUrl =
            (m_storageClient->requirePresignedUrl() && (response.presignedUrls.size() > i)) ?
              response.presignedUrls.at(i) : "";
          EncryptionMaterial *encMat = (response.encryptionMaterials.size() > i) ?
                                 &response.encryptionMaterials.at(i) : NULL;
          size_t getThreshold = DOWNLOAD_DATA_SIZE_THRESHOLD;
          if (m_transferConfig &&
            (m_transferConfig->getSizeThreshold > DOWNLOAD_DATA_SIZE_THRESHOLD))
          {
            CXX_LOG_INFO("Set downloading threshold: %ld", m_transferConfig->getSizeThreshold);
            getThreshold = m_transferConfig->getSizeThreshold;
          }
          RemoteStorageRequestOutcome outcome =
            m_FileMetadataInitializer.populateSrcLocDownloadMetadata(
              sourceLocations->at(i), &response.stageInfo.location,
              m_storageClient, encMat, presignedUrl, getThreshold);

          if (outcome == TOKEN_EXPIRED)
          {
            CXX_LOG_DEBUG("Token expired when getting download metadata");
            this->renewToken(command);
            i--;
          }
          break;
        }
      default:
        CXX_LOG_FATAL(CXX_LOG_NS, "Invalid command type");
        throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
                                         "Invalid command type.");
    }
  }
}

void Snowflake::Client::FileTransferAgent::upload(string *command)
{
	//If source file does not exist then we need at least one m_executionResults.m_outcomes to save the outcome.
  int numFiles = m_largeFilesMeta.size() + m_smallFilesMeta.size();
  numFiles = (numFiles > 0) ? numFiles : 1;
  m_executionResults = new FileTransferExecutionResult(CommandType::UPLOAD, numFiles);

  if (m_largeFilesMeta.size() > 0)
  {
    for (size_t i=0; i<m_largeFilesMeta.size(); i++)
    {
      m_largeFilesMeta[i].overWrite = response.overwrite;
      m_executionResults->SetFileMetadata(&m_largeFilesMeta[i], i);

      CXX_LOG_DEBUG("Putget serial large file upload, %s file", m_largeFilesMeta[i].srcFileName.c_str());
      if (isPutFastFailEnabled() && !m_failedTransfers.empty())
      {
        m_executionResults->SetTransferOutCome(RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE, i);
        CXX_LOG_DEBUG("Sequential upload, put fast fail enabled, Skipping file.");
        continue;
      }
      if (m_storageClient->requirePresignedUrl())
      {
        getPresignedUrlForUploading(m_largeFilesMeta[i], *command);
      }
      RemoteStorageRequestOutcome outcome = uploadSingleFile(m_storageClient,
                                                 &m_largeFilesMeta[i], i);
      m_executionResults->SetTransferOutCome(outcome, i);

      if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
      {
        CXX_LOG_DEBUG("Putget serial large file upload, %s file renewToken", m_largeFilesMeta[i].srcFileName.c_str());
        renewToken(command);
        i--;
      }
      else if( outcome == RemoteStorageRequestOutcome::FAILED)
      {
        m_failedTransfers.append(m_largeFilesMeta[i].srcFileName) + ", ";
        //Fast fail, return when the first file fails to upload.
        if (isPutFastFailEnabled())
        {
          CXX_LOG_DEBUG("Putget serial large file upload, %s file upload FAILED.",
                  m_largeFilesMeta[i].srcFileName.c_str());
        }
      }
      else if( outcome == RemoteStorageRequestOutcome::SUCCESS)
      {
        CXX_LOG_DEBUG("Putget serial large file upload, %s file upload SUCCESS.",
                m_largeFilesMeta[i].srcFileName.c_str());
      }
    }
  }

  if (m_smallFilesMeta.size() > 0)
  {
    //skip getting presigned url if fast fail is enabled and failed already
    if ((!isPutFastFailEnabled()) || m_failedTransfers.empty())
    {
      if (m_storageClient->requirePresignedUrl())
      {
        for (size_t i = 0; i < m_smallFilesMeta.size(); i++)
        {
          std::string localCommand = *command;
          getPresignedUrlForUploading(m_smallFilesMeta[i], localCommand);
        }
      }
    }
    uploadFilesInParallel(command);
  }
  if( m_largeFilesMeta.size() + m_smallFilesMeta.size() == 0)
  {
    CXX_LOG_DEBUG("No files to upload, source files do not exist. put command %s FAILED.", command->c_str());
    m_executionResults->SetTransferOutCome(RemoteStorageRequestOutcome::FAILED, 0);
    throw SnowflakeTransferException(TransferError::FAILED_TO_TRANSFER, "source file does not exist.");
  }

  if (!m_failedTransfers.empty())
  {
    CXX_LOG_DEBUG("%s command FAILED.", command);
    if (isPutFastFailEnabled())
    {
      throw SnowflakeTransferException(TransferError::FAST_FAIL_ENABLED_SKIP_UPLOADS, m_failedTransfers.c_str());
    }
    throw SnowflakeTransferException(TransferError::FAILED_TO_TRANSFER, m_failedTransfers.c_str());
  }
}

void Snowflake::Client::FileTransferAgent::uploadFilesInParallel(std::string *command)
{
  Snowflake::Client::Util::ThreadPool tp((unsigned int)response.parallel);
  for (size_t i=0; i<m_smallFilesMeta.size(); i++)
  {
    size_t resultIndex = i + m_largeFilesMeta.size();
    FileMetadata * metadata = &m_smallFilesMeta[i];
    m_executionResults->SetFileMetadata(&m_smallFilesMeta[i], resultIndex);
    metadata->overWrite = response.overwrite;

    // workaround for incident 00212627
    // Do not use thread pool when parallel = 1
    if ((unsigned int)response.parallel <= 1)
    {
      CXX_LOG_DEBUG("Sequential upload %d th file %s start", i, metadata->srcFileName.c_str());
      do
      {
        RemoteStorageRequestOutcome outcome = RemoteStorageRequestOutcome::SUCCESS;
        if(isPutFastFailEnabled() && !m_failedTransfers.empty())
        {
          m_executionResults->SetTransferOutCome(RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE, resultIndex);
          CXX_LOG_DEBUG("Sequential upload, put fast fail enabled, Skipping file.");
          break;
        }
        outcome = uploadSingleFile(m_storageClient, metadata, resultIndex);
        if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
        {
          CXX_LOG_DEBUG("Sequential upload %d th file %s renewToken", i, metadata->srcFileName.c_str());
          _mutex_lock(&m_parallelTokRenewMutex);
          this->renewToken(command);
          _mutex_unlock(&m_parallelTokRenewMutex);
        }
        else
        {
          if (outcome == RemoteStorageRequestOutcome::FAILED) {
            CXX_LOG_DEBUG("Sequential upload %s FAILED.", metadata->srcFileName.c_str());
            m_failedTransfers.append(metadata->srcFileName) + ", ";
            //Fast fail, return when the first file fails to upload.
            if(isPutFastFailEnabled())
            {
              CXX_LOG_DEBUG("Sequential upload, put fast fail enabled, Skip uploading rest of the files.");
            }
          }
          else if (outcome == RemoteStorageRequestOutcome::SUCCESS) {
            CXX_LOG_DEBUG("Sequential upload %d th file %s SUCCESS.", i, metadata->srcFileName.c_str());
          }
          break;
        }
      } while (true);

      continue;
    }

    tp.AddJob([metadata, resultIndex, command, this]()->void {
        do
        {
          RemoteStorageRequestOutcome outcome = RemoteStorageRequestOutcome::SUCCESS;
          CXX_LOG_DEBUG("Putget Parallel upload %s", metadata->srcFileName.c_str());
          if(isPutFastFailEnabled() && !m_failedTransfers.empty())
          {
              CXX_LOG_DEBUG("Fast fail enabled, One of the threads failed to upload file, "
                            "Quit uploading rest of the files.");
              m_executionResults->SetTransferOutCome(RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE, resultIndex);
              break;
          }
          // SNOW-218025: Upload and download is not exception safe, catch exception
          // and take it as transfer failure.
          try
          {
            outcome = uploadSingleFile(m_storageClient, metadata,
                                                       resultIndex);
            if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
            {
              CXX_LOG_DEBUG("Token expired, Renewing token.")
              _mutex_lock(&m_parallelTokRenewMutex);
              this->renewToken(command);
              _mutex_unlock(&m_parallelTokRenewMutex);

              continue;
            }
          }
          catch (...)
          {
            outcome = RemoteStorageRequestOutcome::FAILED;
          }
          if( outcome == RemoteStorageRequestOutcome::FAILED)
          {
            //Cannot throw and catch error from a thread.
            CXX_LOG_DEBUG("Parallel upload %s FAILED", metadata->srcFileName.c_str());
            _mutex_lock(&m_parallelFailedMsgMutex);
            m_failedTransfers.append(metadata->srcFileName) + ", ";
            _mutex_unlock(&m_parallelFailedMsgMutex);
          }
          else if (outcome == RemoteStorageRequestOutcome::SUCCESS)
          {
            CXX_LOG_DEBUG("Putget Parallel upload %s SUCCESS.", metadata->srcFileName.c_str());
          }
          break;
        } while (true);
    });
  }

  // wait till all jobs have been finished
  tp.WaitAll();
  CXX_LOG_DEBUG("All threads exited, Parallel put done.");
}

void Snowflake::Client::FileTransferAgent::renewToken(std::string *command)
{
  long now = (long)time(NULL);

  // Call parse command only if last token renew time is more than 10 minutes
  // (10 minutes is just a random value I pick, to check if token has been
  //  refreshed "recently")
  // This check is only necessary for multiple thread scenario.
  // In a single thread case, if token is expired, then last time token is
  // refreshed is for sure 4 hour ago. However, in multi thread case, if token
  // is expired but last token update time is within 10 minutes, it is almost for
  // sure that some other thread has already renewed the token
  if (now - m_lastRefreshTokenSec > 10 * 60)
  {
    CXX_LOG_INFO("Renew aws token");
    if (!m_stmtPutGet->parsePutGetCommand(command, &response))
    {
      throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
                                       "Failed to parse response.");
    }
    m_storageClient = StorageClientFactory::getClient(&response.stageInfo,
                                                      (unsigned int) response.parallel,
                                                      response.threshold,
                                                      m_transferConfig,
                                                      m_stmtPutGet);
    m_lastRefreshTokenSec = now;
  }
}

RemoteStorageRequestOutcome Snowflake::Client::FileTransferAgent::uploadSingleFile(
  IStorageClient *client,
  FileMetadata *fileMetadata,
  size_t resultIndex)
{
  // compress if required
  CXX_LOG_DEBUG("Entrance uploadSingleFile");
  if (fileMetadata->requireCompress)
  {
    fileMetadata->recordPutGetTimestamp(FileMetadata::COMP_START);
    compressSourceFile(fileMetadata);
    fileMetadata->recordPutGetTimestamp(FileMetadata::COMP_END);
  } else
  {
    fileMetadata->srcFileToUpload = fileMetadata->srcFileName;
    fileMetadata->srcFileToUploadSize = fileMetadata->srcFileSize;
  }
  CXX_LOG_TRACE("Update File digest metadata start");

  // calculate digest
  updateFileDigest(fileMetadata);
  CXX_LOG_TRACE("Encryption metadata init start");
  m_FileMetadataInitializer.initEncryptionMetadata(fileMetadata);
  CXX_LOG_TRACE("Encryption metadata init done");

  RemoteStorageRequestOutcome outcome = RemoteStorageRequestOutcome::SUCCESS;
  RetryContext putRetryCtx(fileMetadata->srcFileName, m_maxPutRetries);
  do
  {
    //Sleeps only when its a retry
    putRetryCtx.waitForNextRetry();
    std::basic_iostream<char> *srcFileStream;
    ::std::fstream fs;

    if (m_uploadStream) {
      srcFileStream = m_uploadStream;
    } else {
      try {
        fs = ::std::fstream(m_stmtPutGet->UTF8ToPlatformString(fileMetadata->srcFileToUpload).c_str(),
                            ::std::ios_base::in |
                            ::std::ios_base::binary);
      }
      catch (...) {
        std::string err = "Could not open source file " + fileMetadata->srcFileToUpload;
        throw SnowflakeTransferException(TransferError::FAILED_TO_TRANSFER, err.c_str());
      }
      srcFileStream = &fs;
    }

    if (fileMetadata->encryptionMetadata.fileKey.nbBits > 0)
    {
      Crypto::CipherIOStream inputEncryptStream(*srcFileStream,
                                                Crypto::CryptoOperation::ENCRYPT,
                                                fileMetadata->encryptionMetadata.fileKey,
                                                fileMetadata->encryptionMetadata.iv,
                                                FILE_ENCRYPTION_BLOCK_SIZE);

      fileMetadata->recordPutGetTimestamp(FileMetadata::PUT_START);
      // upload stream
      outcome = client->upload(fileMetadata, &inputEncryptStream);
    }
    else
    {
      // server side encryption
      fileMetadata->recordPutGetTimestamp(FileMetadata::PUT_START);
      // upload stream
      outcome = client->upload(fileMetadata, srcFileStream);
    }
    fileMetadata->recordPutGetTimestamp(FileMetadata::PUT_END);
    CXX_LOG_DEBUG("File upload done.");
    if (fs.is_open())
    {
      fs.close();
    }

    m_executionResults->SetTransferOutCome(outcome, resultIndex);
    fileMetadata->recordPutGetTimestamp(FileMetadata::PUTGET_END);
    fileMetadata->printPutGetTimestamp();
  } while (putRetryCtx.isRetryable(outcome));
  CXX_LOG_DEBUG("Exit UploadSingleFile");
  return outcome;
}

void Snowflake::Client::FileTransferAgent::updateFileDigest(
  FileMetadata *fileMetadata)
{
  const int CHUNK_SIZE = 16 * 4 * 1024;

  std::basic_iostream<char> *srcFileStream;
  ::std::fstream fs;

  if (m_uploadStream)
  {
    srcFileStream = m_uploadStream;
  }
  else
  {
    fs = ::std::fstream(fileMetadata->srcFileToUpload,
      ::std::ios_base::in | ::std::ios_base::binary);
    srcFileStream = &fs;
  }

  Crypto::HashContext hashContext(Crypto::Cryptor::getInstance()
                                    .createHashContext(
                                      Crypto::CryptoHashFunc::SHA256));

  const size_t digestSize = Crypto::cryptoHashDigestSize(
    Crypto::CryptoHashFunc::SHA256);
  char digest[digestSize];

  hashContext.initialize();

  char sourceFileBuffer[CHUNK_SIZE];
  while (srcFileStream->read(sourceFileBuffer, CHUNK_SIZE))
  {
    hashContext.next(sourceFileBuffer, CHUNK_SIZE);
  }

  if (fs.is_open())
  {
    fs.close();
  }
  
  if (m_uploadStream)
  {
    m_uploadStream->clear();
    m_uploadStream->seekg(0, std::ios::beg);
  }

  hashContext.finalize(digest);

  const size_t digestEncodeSize = Util::Base64::encodedLength(digestSize);
  char digestEncode[digestEncodeSize];
  Util::Base64::encode(digest, digestSize, digestEncode);

  fileMetadata->sha256Digest = string(digestEncode, digestEncodeSize);
}

void Snowflake::Client::FileTransferAgent::compressSourceFile(
  FileMetadata *fileMetadata)
{
  CXX_LOG_DEBUG("Starting file compression");
  
  char tempDir[MAX_PATH]={0};
  int level = -1;
  if (m_transferConfig)
  {
    level = m_transferConfig->compressLevel;
    if (m_transferConfig->tempDir)
    {
      sb_strcat(tempDir, sizeof(tempDir), m_transferConfig->tempDir);
    }
  }
  sf_get_uniq_tmp_dir(tempDir);
  // tempDir could be passed in from application. Check if the folder can be
  // created successfully to prevent command injection.
  if (!sf_is_directory_exist(tempDir))
  {
    CXX_LOG_ERROR("Failed to create temporary folder %s. Errno: %d", tempDir, errno);
    throw SnowflakeTransferException(TransferError::FILE_OPEN_ERROR, tempDir, -1);
  }

  std::string stagingFile(tempDir);
  stagingFile += m_stmtPutGet->UTF8ToPlatformString(fileMetadata->destFileName);
  std::string srcFileNamePlatform = m_stmtPutGet->UTF8ToPlatformString(fileMetadata->srcFileName);

  FILE *sourceFile = NULL;
  if (sb_fopen(&sourceFile, srcFileNamePlatformta->srcFileName.c_str(), "r") == NULL) {
    CXX_LOG_ERROR("Failed to open srcFileName %s. Errno: %d", fileMetadata->srcFileName.c_str(), errno);
    throw SnowflakeTransferException(TransferError::FILE_OPEN_ERROR, srcFileNamePlatform.c_str(), -1);
  }
  FILE *destFile = NULL;
  if (sb_fopen(&destFile, stagingFile.c_str(), "w") == NULL) {
    CXX_LOG_ERROR("Failed to open srcFileToUpload file %s. Errno: %d", stagingFile.c_str(), errno);
    throw SnowflakeTransferException(TransferError::FILE_OPEN_ERROR, stagingFile.c_str(), -1);
  }
  // set srcFileToUpload after open file successfully to prevent command injection.
  fileMetadata->srcFileToUpload = m_stmtPutGet->platformStringToUTF8(stagingFile);

  int ret = Util::CompressionUtil::compressWithGzip(sourceFile, destFile,
                                          fileMetadata->srcFileToUploadSize, level);
  fclose(sourceFile);
  fclose(destFile);

  if (ret != 0)
  {
    CXX_LOG_ERROR("Failed to compress source file. Error code: %d", ret);
    throw SnowflakeTransferException(TransferError::COMPRESSION_ERROR, "Failed to compress source file", ret);
  }
}

void Snowflake::Client::FileTransferAgent::download(string *command)
{
  m_executionResults = new FileTransferExecutionResult(CommandType::DOWNLOAD,
    m_largeFilesMeta.size() + m_smallFilesMeta.size());

  int ret = sf_create_directory_if_not_exists((const char *)response.localLocation);
  if (ret != 0)
  {
    CXX_LOG_ERROR("Filed to create directory %s", response.localLocation);
    throw SnowflakeTransferException(TransferError::MKDIR_ERROR, 
      response.localLocation, ret);
  }

  if (m_largeFilesMeta.size() > 0)
  {
    for (size_t i=0; i<m_largeFilesMeta.size(); i++)
    {
      m_executionResults->SetFileMetadata(&m_largeFilesMeta[i], i);
      CXX_LOG_DEBUG("Putget serial large file download, %s file",
                    m_largeFilesMeta[i].srcFileName.c_str());
      if (isGetFastFailEnabled() && !m_failedTransfers.empty())
      {
        m_executionResults->SetTransferOutCome(RemoteStorageRequestOutcome::FAILED, i);
        CXX_LOG_DEBUG("Sequential download, get fast fail enabled, Skipping file.");
        continue;
      }
      RemoteStorageRequestOutcome outcome = downloadSingleFile(m_storageClient,
                                                               &m_largeFilesMeta[i], i);

      if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
      {
        CXX_LOG_DEBUG("Putget serial large file download, %s file renewToken",
                      m_largeFilesMeta[i].srcFileName.c_str());
        renewToken(command);
        i--;
      }
      else if (outcome == RemoteStorageRequestOutcome::FAILED)
      {
        CXX_LOG_DEBUG("Putget serial large file download, %s file download FAILED.",
                      m_largeFilesMeta[i].srcFileName.c_str());
        m_failedTransfers.append(m_largeFilesMeta[i].srcFileName) + ", ";
        //Fast fail, return when the first file fails to download.
        if (isGetFastFailEnabled())
        {
          CXX_LOG_DEBUG("Sequential download, get fast fail enabled, Skip downloading rest of the files.");
        }
      }
      else if (outcome == RemoteStorageRequestOutcome::SUCCESS)
      {
        CXX_LOG_DEBUG("Putget serial large file download, %s file download SUCCESS.",
                      m_largeFilesMeta[i].srcFileName.c_str());
      }
    }
  }

  if (m_smallFilesMeta.size() > 0)
  {
    downloadFilesInParallel(command);
  }

  if (!m_failedTransfers.empty())
  {
    CXX_LOG_DEBUG("%s command FAILED.", command);
    if (isGetFastFailEnabled())
    {
      throw SnowflakeTransferException(TransferError::FAST_FAIL_ENABLED_SKIP_DOWNLOADS, m_failedTransfers.c_str());
    }
  }
}

void Snowflake::Client::FileTransferAgent::downloadFilesInParallel(std::string *command)
{
  Snowflake::Client::Util::ThreadPool tp((unsigned int)response.parallel);
  for (size_t i=0; i<m_smallFilesMeta.size(); i++)
  {
    size_t resultIndex = i + m_largeFilesMeta.size();
    FileMetadata * metadata = &m_smallFilesMeta[i];
    m_executionResults->SetFileMetadata(&m_smallFilesMeta[i], resultIndex);

    // workaround for incident 00212627
    // Do not use thread pool when parallel = 1
    if ((unsigned int)response.parallel <= 1)
    {
      do
      {
        CXX_LOG_DEBUG("Sequential download %d th file %s start",
                      i, metadata->srcFileName.c_str());
        RemoteStorageRequestOutcome outcome = RemoteStorageRequestOutcome::SUCCESS;
        if (isGetFastFailEnabled() && !m_failedTransfers.empty())
        {
          m_executionResults->SetTransferOutCome(RemoteStorageRequestOutcome::FAILED, resultIndex);
          CXX_LOG_DEBUG("Sequential download, get fast fail enabled, Skipping file.");
          break;
        }
        outcome = downloadSingleFile(m_storageClient, metadata, resultIndex);
        if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
        {
          CXX_LOG_DEBUG("Sequential download %d th file %s renewToken",
                        i, metadata->srcFileName.c_str());
          _mutex_lock(&m_parallelTokRenewMutex);
          this->renewToken(command);
          _mutex_unlock(&m_parallelTokRenewMutex);
        }
        else if (outcome == RemoteStorageRequestOutcome::FAILED)
        {
          CXX_LOG_DEBUG("Sequential download %s FAILED.", metadata->srcFileName.c_str());
          m_failedTransfers.append(metadata->srcFileName) + ", ";
          //Fast fail, return when the first file fails to download.
          if (isGetFastFailEnabled())
          {
            CXX_LOG_DEBUG("Sequential download, get fast fail enabled, Skip downloading rest of the files.");
          }
          break;
        }
        else if (outcome == RemoteStorageRequestOutcome::SUCCESS)
        {
          CXX_LOG_DEBUG("Sequential download %d th file %s SUCCESS.", i, metadata->srcFileName.c_str());
          break;
        }
      } while (true);

      continue;
    }

    tp.AddJob([metadata, resultIndex, command, this]()->void {
        do
        {
          // SNOW-218025: Upload and download is not exception safe, catch exception
          // and take it as transfer failure.
          try
          {
            RemoteStorageRequestOutcome outcome = RemoteStorageRequestOutcome::SUCCESS;
            CXX_LOG_DEBUG("Putget Parallel download %s", metadata->srcFileName.c_str());
            if (isGetFastFailEnabled() && !m_failedTransfers.empty())
            {
              m_executionResults->SetTransferOutCome(RemoteStorageRequestOutcome::FAILED, resultIndex);
              CXX_LOG_DEBUG("Fast fail enabled, One of the threads failed to download file, "
                            "Quit downloading rest of the files.");
              break;
            }
            outcome = downloadSingleFile(m_storageClient, metadata, resultIndex);
            if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
            {
              CXX_LOG_DEBUG("Token expired, Renewing token.")
              _mutex_lock(&m_parallelTokRenewMutex);
              this->renewToken(command);
              _mutex_unlock(&m_parallelTokRenewMutex);

              continue;
            }
            else if (outcome == RemoteStorageRequestOutcome::FAILED)
            {
              CXX_LOG_DEBUG("Parallel download %s FAILED.", metadata->srcFileName.c_str());
              _mutex_lock(&m_parallelFailedMsgMutex);
              m_failedTransfers.append(metadata->srcFileName) + ", ";
              _mutex_unlock(&m_parallelFailedMsgMutex);
            }
            else if (outcome == RemoteStorageRequestOutcome::SUCCESS)
            {
              CXX_LOG_DEBUG("Parallel download file %s SUCCESS.", metadata->srcFileName.c_str());
            }
          }
          catch (...)
          {
            m_executionResults->SetTransferOutCome(
              RemoteStorageRequestOutcome::FAILED, resultIndex);
            CXX_LOG_DEBUG("Parallel download %s FAILED.", metadata->srcFileName.c_str());
            _mutex_lock(&m_parallelFailedMsgMutex);
            m_failedTransfers.append(metadata->srcFileName) + ", ";
            _mutex_unlock(&m_parallelFailedMsgMutex);
          }

          break;
        } while (true);
    });
  }

  // wait till all jobs have been finished
  tp.WaitAll();
  CXX_LOG_DEBUG("All threads exited, Parallel get done.");
}

RemoteStorageRequestOutcome Snowflake::Client::FileTransferAgent::downloadSingleFile(
  IStorageClient *client,
  FileMetadata *fileMetadata,
  size_t resultIndex)
{
   fileMetadata->destPath = std::string(response.localLocation) + PATH_SEP +
    fileMetadata->destFileName;
   std::string destPathPlatform = m_stmtPutGet->UTF8ToPlatformString(fileMetadata->destPath);

   RemoteStorageRequestOutcome outcome = RemoteStorageRequestOutcome::FAILED;
   RetryContext getRetryCtx(fileMetadata->srcFileName, m_maxGetRetries);
   do
   {
     //Sleeps only when its a retry
     getRetryCtx.waitForNextRetry();

     std::basic_fstream<char> dstFile;
     try {
       dstFile = std::basic_fstream<char>(destPathPlatform.c_str(),
                                          std::ios_base::out | std::ios_base::binary);
     }
     catch (...) {
       std::string err = "Could not open file " + fileMetadata->destPath + " to downoad";
       CXX_LOG_DEBUG("Could not open file %s to downoad: %s",
                     fileMetadata->destPath.c_str(), sf_strerror(errno));
       m_executionResults->SetTransferOutCome(outcome, resultIndex);
       break;
     }
     if (!dstFile.is_open())
     {
       std::string err = "Could not open file " + fileMetadata->destPath + " to downoad";
       CXX_LOG_DEBUG("Could not open file %s to downoad: %s",
                     fileMetadata->destPath.c_str(), sf_strerror(errno));
       m_executionResults->SetTransferOutCome(outcome, resultIndex);
       break;
     }
     if (fileMetadata->encryptionMetadata.fileKey.nbBits > 0)
     {
       Crypto::CipherIOStream decryptOutputStream(
         dstFile,
         Crypto::CryptoOperation::DECRYPT,
         fileMetadata->encryptionMetadata.fileKey,
         fileMetadata->encryptionMetadata.iv,
         FILE_ENCRYPTION_BLOCK_SIZE);

       outcome = client->download(fileMetadata, &decryptOutputStream);
     }
     else
     {
       // server side encryption
       outcome = client->download(fileMetadata, &dstFile);
     }
     dstFile.close();
     m_executionResults->SetTransferOutCome(outcome, resultIndex);
   } while (getRetryCtx.isRetryable(outcome));

  return outcome;
}

void Snowflake::Client::FileTransferAgent::getPresignedUrlForUploading(
                               FileMetadata& fileMetadata,
                               const std::string& command)
{
  // need to replace file://mypath/myfile?.csv with file://mypath/myfile1.csv.gz
  std::string presignedUrlCommand = command;
  std::string localFilePath = getLocalFilePathFromCommand(command, false);
  replaceStrAll(presignedUrlCommand, localFilePath, fileMetadata.destFileName);
  // then hand that to GS to get the actual presigned URL we'll use
  PutGetParseResponse rsp;
  if (!m_stmtPutGet->parsePutGetCommand(&presignedUrlCommand, &rsp))
  {
    throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
      "Failed to parse response.");
  }
  CXX_LOG_INFO("Parse response succeed");

  fileMetadata.presignedUrl = rsp.stageInfo.presignedUrl;
}

std::string Snowflake::Client::FileTransferAgent::getLocalFilePathFromCommand(
                               std::string const& command,
                               bool unescape)
{
  if (std::string::npos == command.find(FILE_PROTOCOL))
  {
    CXX_LOG_ERROR("file:// prefix not found in command %s", command.c_str());
    throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
      "file:// prefix not found in command.");
  }

  int localFilePathBeginIdx = command.find(FILE_PROTOCOL) +
    FILE_PROTOCOL.length();
  bool isLocalFilePathQuoted =
    (localFilePathBeginIdx > FILE_PROTOCOL.length()) &&
    (command.at(localFilePathBeginIdx - 1 - FILE_PROTOCOL.length()) == '\'');

  // the ending index is exclusive
  int localFilePathEndIdx = 0;
  std::string localFilePath = "";

  if (isLocalFilePathQuoted)
  {
    // look for the matching quote
    localFilePathEndIdx = command.find("'", localFilePathBeginIdx);
    if (localFilePathEndIdx > localFilePathBeginIdx)
    {
      localFilePath = command.substr(localFilePathBeginIdx,
        localFilePathEndIdx - localFilePathBeginIdx);
    }
    // unescape backslashes to match the file name from GS
    if (unescape)
    {
      replaceStrAll(localFilePath, "\\\\\\\\", "\\\\");
    }
  }
  else
  {
    // look for the first space or new line or semi colon
    localFilePathEndIdx = command.find_first_of(" \n;", localFilePathBeginIdx);

    if (localFilePathEndIdx > localFilePathBeginIdx)
    {
      localFilePath = command.substr(localFilePathBeginIdx,
        localFilePathEndIdx - localFilePathBeginIdx);
    }
    else if (std::string::npos == localFilePathEndIdx)
    {
      localFilePath = command.substr(localFilePathBeginIdx);
    }
  }

  return localFilePath;
}
