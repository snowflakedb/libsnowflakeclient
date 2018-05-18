/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <memory>
#include <fstream>
#include <iostream>
#include <vector>
#include "FileTransferAgent.hpp"
#include "util/Base64.hpp"
#include "SnowflakeS3Client.hpp"
#include "StorageClientFactory.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include "crypto/Cryptor.hpp"
#include "util/CompressionUtil.hpp"
#include "util/ThreadPool.hpp"
#include "EncryptionProvider.hpp"

using ::std::string;
using ::std::vector;

Snowflake::Client::FileTransferAgent::FileTransferAgent(
  IStatementPutGet *statement) :
  m_stmtPutGet(statement),
  m_FileMetadataInitializer(&m_smallFilesMeta, &m_largeFilesMeta),
  m_executionResults(nullptr),
  m_storageClient(nullptr)
{
}

Snowflake::Client::FileTransferAgent::~FileTransferAgent()
{
  reset();
}

void Snowflake::Client::FileTransferAgent::reset()
{
  if (m_executionResults != nullptr)
  {
    delete m_executionResults;
  }

  if (m_storageClient != nullptr)
  {
    delete m_storageClient;
  }

  m_largeFilesMeta.clear();
  m_smallFilesMeta.clear();
}

FileTransferExecutionResult *
Snowflake::Client::FileTransferAgent::execute(string *command)
{
  reset();

  // first parse command
  if (!m_stmtPutGet->parsePutGetCommand(command, &response))
  {
    //TODO finalize exception;
    throw;
  }
  sf_log_info(CXX_LOG_NS, "Parse response succeed.");

  // init storage client
  m_storageClient = StorageClientFactory
  ::getClient(response.getStageInfo(), (unsigned int)response.getParallel());

  // init file metadata
  initFileMetadata();

  switch (response.getCommandType())
  {
    case CommandType::UPLOAD:
      upload(command);
      break;

    case CommandType::DOWNLOAD:
      download(command);
      break;

    default:
      throw;
  }

  return m_executionResults;
}

void Snowflake::Client::FileTransferAgent::initFileMetadata()
{
  m_FileMetadataInitializer.setAutoCompress(response.getAutoCompress());
  m_FileMetadataInitializer.setSourceCompression(response.getSourceCompression());
  m_FileMetadataInitializer.setEncryptionMaterial(
    response.getEncryptionMaterial());

  vector<string> *sourceLocations = response.getSourceLocations();
  for (size_t i = 0; i < sourceLocations->size(); i++)
  {
    switch (response.getCommandType())
    {
      case CommandType::UPLOAD:
        m_FileMetadataInitializer.populateSrcLocUploadMetadata(
          sourceLocations->at(i));
        break;
      case CommandType::DOWNLOAD:
        //TODO handle token renew
        m_FileMetadataInitializer.populateSrcLocDownloadMetadata(
          sourceLocations->at(i), response.getStageInfo()->getLocation(),
          m_storageClient);
        break;
      default:
        sf_log_error(CXX_LOG_NS, "Invalid command type");
    }
  }
}

void Snowflake::Client::FileTransferAgent::upload(string *command)
{
  m_executionResults = new FileTransferExecutionResult(CommandType::UPLOAD,
    m_largeFilesMeta.size() + m_smallFilesMeta.size());

  if (m_largeFilesMeta.size() > 0)
  {
    for (size_t i=0; i<m_largeFilesMeta.size(); i++)
    {
      m_executionResults->SetFileMetadata(&m_largeFilesMeta[i], i);
      RemoteStorageRequestOutcome outcome = uploadSingleFile(m_storageClient,
                                                 &m_largeFilesMeta[i], i);

      if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
      {
        renewToken(command);
        i--;
      }
    }
  }

  if (m_smallFilesMeta.size() > 0)
  {
    uploadFilesInParallel(command);
  }
}

void Snowflake::Client::FileTransferAgent::uploadFilesInParallel(std::string *command)
{
  SF_MUTEX_HANDLE tokenRenewMutex;
  _mutex_init(&tokenRenewMutex);

  Snowflake::Client::Util::ThreadPool tp((unsigned int)response.getParallel());
  for (size_t i=0; i<m_smallFilesMeta.size(); i++)
  {
    unsigned int resultIndex = i + m_largeFilesMeta.size();
    FileMetadata * metadata = &m_smallFilesMeta[i];
    m_executionResults->SetFileMetadata(&m_smallFilesMeta[i], resultIndex);
    tp.AddJob([metadata, resultIndex, &tokenRenewMutex, command, this]()->void {
        do
        {
          RemoteStorageRequestOutcome outcome = uploadSingleFile(m_storageClient, metadata,
                                                     resultIndex);
          if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
          {
            _mutex_lock(&tokenRenewMutex);
            this->renewToken(command);
            _mutex_unlock(&tokenRenewMutex);
          }
          else
          {
            break;
          }
        } while (true);
    });
  }

  // wait till all jobs have been finished
  tp.WaitAll();

  _mutex_term(&tokenRenewMutex);
}

void Snowflake::Client::FileTransferAgent::renewToken(std::string *command)
{
  m_stmtPutGet->parsePutGetCommand(command, &response);
  m_storageClient = StorageClientFactory::getClient(response.getStageInfo(),
                                           (unsigned int)response.getParallel());
}

RemoteStorageRequestOutcome Snowflake::Client::FileTransferAgent::uploadSingleFile(
  IStorageClient *client,
  FileMetadata *fileMetadata,
  unsigned int resultIndex)
{
  // compress if required
  if (fileMetadata->requireCompress)
  {
    compressSourceFile(fileMetadata);
  } else
  {
    fileMetadata->srcFileToUpload = fileMetadata->srcFileName;
    fileMetadata->srcFileToUploadSize = fileMetadata->srcFileSize;
  }

  // calculate digest
  updateFileDigest(fileMetadata);

  ::std::fstream originalFileStream(fileMetadata->srcFileToUpload.c_str(),
                                    ::std::ios_base::in |
                                    ::std::ios_base::binary);

  m_FileMetadataInitializer.initEncryptionMetadata(fileMetadata);

  Crypto::CipherIOStream inputEncryptStream(originalFileStream,
    Crypto::CryptoOperation::ENCRYPT,
    fileMetadata->encryptionMetadata.fileKey,
    fileMetadata->encryptionMetadata.iv,
    128);

  // upload stream
  RemoteStorageRequestOutcome outcome = client->upload(fileMetadata,
                                                       &inputEncryptStream);
  originalFileStream.close();

  if (fileMetadata->requireCompress)
  {
    remove(fileMetadata->srcFileToUpload.c_str());
  }

  m_executionResults->SetTransferOutCome(outcome, resultIndex);
  return outcome;
}

void Snowflake::Client::FileTransferAgent::updateFileDigest(
  FileMetadata *fileMetadata)
{
  const int CHUNK_SIZE = 16 * 4 * 1024;

  ::std::fstream fs(fileMetadata->srcFileToUpload,
                    ::std::ios_base::in | ::std::ios_base::binary);

  Crypto::HashContext hashContext(Crypto::Cryptor::getInstance()
                                    .createHashContext(
                                      Crypto::CryptoHashFunc::SHA256));

  const size_t digestSize = Crypto::cryptoHashDigestSize(
    Crypto::CryptoHashFunc::SHA256);
  char digest[digestSize];

  hashContext.initialize();

  char sourceFileBuffer[CHUNK_SIZE];
  while (fs.read(sourceFileBuffer, CHUNK_SIZE))
  {
    hashContext.next(sourceFileBuffer, CHUNK_SIZE);
  }
  fs.close();

  hashContext.finalize(digest);

  const size_t digestEncodeSize = Util::Base64::encodedLength(digestSize);
  char digestEncode[digestEncodeSize];
  Util::Base64::encode(digest, digestSize, digestEncode);

  fileMetadata->sha256Digest = string(digestEncode, digestEncodeSize);
}

void Snowflake::Client::FileTransferAgent::compressSourceFile(
  FileMetadata *fileMetadata)
{
  //TODO Better handle tmp directory
  fileMetadata->srcFileToUpload = "/tmp/" + fileMetadata->destFileName;

  FILE *sourceFile = fopen(fileMetadata->srcFileName.c_str(), "r");
  FILE *destFile = fopen(fileMetadata->srcFileToUpload.c_str(), "w");

  Util::CompressionUtil::compressWithGzip(sourceFile, destFile,
                                          fileMetadata->srcFileToUploadSize);

  fclose(sourceFile);
  fclose(destFile);
}

void Snowflake::Client::FileTransferAgent::download(string *command)
{
  m_executionResults = new FileTransferExecutionResult(CommandType::DOWNLOAD,
    m_largeFilesMeta.size() + m_smallFilesMeta.size());

  std::string createDirIfNotExist = "mkdir -p " +
    std::string(response.GetLocalLocation());
  const int err = system(createDirIfNotExist.c_str());
  if (err != 0)
  {
    throw;
  }

  if (m_largeFilesMeta.size() > 0)
  {
    for (size_t i=0; i<m_largeFilesMeta.size(); i++)
    {
      m_executionResults->SetFileMetadata(&m_largeFilesMeta[i], i);
      RemoteStorageRequestOutcome outcome = downloadSingleFile(m_storageClient,
                                                               &m_largeFilesMeta[i], i);

      if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
      {
        renewToken(command);
        i--;
      }
    }
  }

  if (m_smallFilesMeta.size() > 0)
  {
    downloadFilesInParallel(command);
  }
}

void Snowflake::Client::FileTransferAgent::downloadFilesInParallel(std::string *command)
{
  SF_MUTEX_HANDLE tokenRenewMutex;
  _mutex_init(&tokenRenewMutex);

  Snowflake::Client::Util::ThreadPool tp((unsigned int)response.getParallel());
  for (size_t i=0; i<m_smallFilesMeta.size(); i++)
  {
    unsigned int resultIndex = i + m_largeFilesMeta.size();
    FileMetadata * metadata = &m_smallFilesMeta[i];
    m_executionResults->SetFileMetadata(&m_smallFilesMeta[i], resultIndex);
    tp.AddJob([metadata, resultIndex, &tokenRenewMutex, command, this]()->void {
        do
        {
          RemoteStorageRequestOutcome outcome = downloadSingleFile(m_storageClient, metadata,
                                                                 resultIndex);
          if (outcome == RemoteStorageRequestOutcome::TOKEN_EXPIRED)
          {
            _mutex_lock(&tokenRenewMutex);
            this->renewToken(command);
            _mutex_unlock(&tokenRenewMutex);
          }
          else
          {
            break;
          }
        } while (true);
    });
  }

  // wait till all jobs have been finished
  tp.WaitAll();

  _mutex_term(&tokenRenewMutex);
}

RemoteStorageRequestOutcome Snowflake::Client::FileTransferAgent::downloadSingleFile(
  IStorageClient *client,
  FileMetadata *fileMetadata,
  unsigned int resultIndex)
{
  EncryptionProvider::decryptFileKey(fileMetadata, response.getEncryptionMaterial());
  std::string dstNameFull = std::string(response.GetLocalLocation()) + "/" +
    fileMetadata->destFileName;

  std::basic_fstream<char> dstFile(dstNameFull.c_str(),
                                std::ios_base::out | std::ios_base::binary);

  Crypto::CipherIOStream decryptOutputStream(
                               dstFile,
                               Crypto::CryptoOperation::DECRYPT,
                               fileMetadata->encryptionMetadata.fileKey,
                               fileMetadata->encryptionMetadata.iv, 128);

  RemoteStorageRequestOutcome outcome = client->download(fileMetadata,
                                                         &decryptOutputStream);

  dstFile.close();

  m_executionResults->SetTransferOutCome(outcome, resultIndex);
  return outcome;
}
