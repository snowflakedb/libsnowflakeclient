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

using ::std::string;
using ::std::vector;

Snowflake::Client::FileTransferAgent::FileTransferAgent(
  IStatementPutGet *statement) :
  m_stmtPutGet(statement),
  m_FileMetadataInitializer(&m_smallFilesMeta, &m_largeFilesMeta)
{
}

Snowflake::Client::FileTransferAgent::~FileTransferAgent()
{
}

FileTransferExecutionResult *
Snowflake::Client::FileTransferAgent::execute(string *command)
{
  // first parse command
  if (!m_stmtPutGet->parsePutGetCommand(command, &response))
  {
    //TODO finalize exception;
    throw;
  }

  // init file metadata
  initFileMetadata();

  switch (response.getCommand())
  {
    case CommandType::UPLOAD:
      upload(response.getStageInfo());
      break;

    case CommandType::DOWNLOAD:
      //download();
      break;

    default:
      throw;
  }

  return nullptr;
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
    m_FileMetadataInitializer.populateSrcLocMetadata(sourceLocations->at(i));
  }
}

void Snowflake::Client::FileTransferAgent::upload(StageInfo *stageInfo)
{
  std::shared_ptr<IStorageClient> storageClient = StorageClientFactory
  ::getClient(stageInfo, response.getParallel());

  if (m_largeFilesMeta.size() > 0)
  {
    for (auto it = m_largeFilesMeta.begin(); it != m_largeFilesMeta.end(); it++)
    {
      executionResults.emplace_back(&(*it), CommandType::UPLOAD);
      uploadSingleFile(storageClient.get(), &(*it), &executionResults.back());
    }
  }

  if (m_smallFilesMeta.size() > 0)
  {
    Snowflake::Client::Util::ThreadPool tp((unsigned int)response.getParallel());
    for (size_t i=0; i<m_smallFilesMeta.size(); i++)
    {
      FileMetadata * metadata = &m_smallFilesMeta[i];
      executionResults.emplace_back(metadata, CommandType::UPLOAD);
      tp.AddJob([storageClient, metadata, i, this]()->void {
        FileTransferExecutionResult * result = &executionResults[i];
        uploadSingleFile(storageClient.get(), metadata, result);
      });
    }

    // wait till all jobs have been finished
    tp.WaitAll();
  }
}

void Snowflake::Client::FileTransferAgent::uploadSingleFile(IStorageClient *client,
  FileMetadata *fileMetadata,
  FileTransferExecutionResult *result)
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
  Crypto::CipherStreamBuf streamBuf(originalFileStream.rdbuf(),
                                    Crypto::CryptoOperation::ENCRYPT,
                                    fileMetadata->encryptionMetadata.fileKey,
                                    fileMetadata->encryptionMetadata.iv,
                                    128);
  std::basic_iostream<char> dataStream(&streamBuf);

  // upload stream
  TransferOutcome outcome = client->upload(fileMetadata, &dataStream);
  originalFileStream.close();

  // wrap execution result and return
  switch (outcome)
  {
    case SUCCESS:
    case SKIPPED:
      if (outcome == SUCCESS && fileMetadata->requireCompress)
      {
        // clean up compressed tmp file
        remove(fileMetadata->srcFileToUpload.c_str());
      }
      result->SetTransferOutCome(outcome);
      break;
    case TOKEN_RENEW:
      //TODO handle token_renew
      break;
    default:
      throw;
  }
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
