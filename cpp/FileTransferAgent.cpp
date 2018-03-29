/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <memory>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <vector>
#include "FileTransferAgent.hpp"
#include "util/Base64.hpp"
#include "SnowflakeS3Client.hpp"
#include "StorageClientFactory.hpp"
#include "EncryptionProvider.hpp"
#include "crypto/CipherStream.hpp"
#include "crypto/Cryptor.hpp"
#include "util/CompressionUtil.hpp"

// used to decide whether to upload in sequence or in parallel
#define DATA_SIZE_THRESHOLD 5242880

using ::std::string;
using ::std::vector;


Snowflake::Client::FileTransferAgent::FileTransferAgent(
  IStatementPutGet *statement) :
  m_stmtPutGet(statement)
{
}

Snowflake::Client::FileTransferAgent::~FileTransferAgent()
{
  clearResults();
}

FileTransferExecutionResult *
Snowflake::Client::FileTransferAgent::execute(string *command)
{
  // clear results if any
  clearResults();

  // first parse command
  PutGetParseResponse response;
  if (!m_stmtPutGet->parsePutGetCommand(command, &response))
  {
    //TODO finalize exception;
    throw;
  }

  // init file metadata
  initFileMetadata(&response);

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

void Snowflake::Client::FileTransferAgent::initFileMetadata(
  PutGetParseResponse *putGetParseResponse)
{
  vector<string> *sourceLocations = putGetParseResponse->getSourceLocations();

  for (size_t i = 0; i < sourceLocations->size(); i++)
  {
    string &fileName = sourceLocations->at(i);
    struct stat fileStatus;
    if (stat(fileName.c_str(), &fileStatus) == 0)
    {
      FileMetadata fileMetadata = {};
      fileMetadata.srcFileName = string(fileName);
      fileMetadata.srcFileSize = (long) fileStatus.st_size;
      fileMetadata.encMat = putGetParseResponse->getEncryptionMaterial();
      fileMetadata.stageInfo = putGetParseResponse->getStageInfo();
      fileMetadata.destFileName = fileName.substr(
        fileName.find_last_of('/') + 1);
      fileMetadata.sourceCompression = putGetParseResponse->getSourceCompression();

      // process compression type
      processCompressionType(&fileMetadata);

      //TODO for now always upload in sequence, will add support to upload in
      //TODO parallel later
      m_largeFilesMeta[string(fileName)] = fileMetadata;

      /*if (fileMetadata.srcFileSize > DATA_SIZE_THRESHOLD)
      {
        m_largeFilesMeta[string(fileName)] = fileMetadata;
      }
      else
      {
        m_smallFilesMeta[string(fileName)] = fileMetadata;
      }*/
    } else
    {
      // TODO throw exception
      throw;
    }
  }
}

void Snowflake::Client::FileTransferAgent::upload(StageInfo *stageInfo)
{
  IStorageClient *storageClient = StorageClientFactory
  ::getClient(stageInfo);

  if (m_largeFilesMeta.size() > 0)
  {
    for (auto it = m_largeFilesMeta.begin(); it != m_largeFilesMeta.end(); it++)
    {
      FileTransferExecutionResult *result =
        uploadSingleFile(storageClient, &it->second);

      executionResults.push_back(result);
    }
  }

  if (m_smallFilesMeta.size() > 0)
  {
    //TODO create thread pool to do upload
  }

  // cleanup
  delete storageClient;
}

FileTransferExecutionResult *
Snowflake::Client::FileTransferAgent::uploadSingleFile(IStorageClient *client,
                                                       FileMetadata *fileMetadata)
{
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

  // encrypt file stream
  EncryptionProvider::updateEncryptionMetadata(fileMetadata);
  Crypto::CipherStream encryptedStream(originalFileStream,
                                       Crypto::CryptoOperation::ENCRYPT,
                                       fileMetadata->encryptionMetadata.fileKey,
                                       fileMetadata->encryptionMetadata.iv);

  // upload stream
  TransferOutcome outcome = client->upload(fileMetadata, &encryptedStream);
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

      return new FileTransferExecutionResult(fileMetadata,
                                             CommandType::UPLOAD, outcome);

    case TOKEN_RENEW:
      //TODO handle token_renew
      break;
    default:
      throw;
  }
}

void Snowflake::Client::FileTransferAgent::processCompressionType(
  FileMetadata *fileMetadata)
{
  //TODO Support auto detect For now just support auto compress by gzip
  fileMetadata->requireCompress = true;
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
  fileMetadata->destFileName += ".gz";

  //TODO Better handle tmp directory
  fileMetadata->srcFileToUpload = "/tmp/" + fileMetadata->destFileName;

  FILE *sourceFile = fopen(fileMetadata->srcFileName.c_str(), "r");
  FILE *destFile = fopen(fileMetadata->srcFileToUpload.c_str(), "w");

  Util::CompressionUtil::compressWithGzip(sourceFile, destFile,
                                          fileMetadata->srcFileToUploadSize);

  fclose(sourceFile);
  fclose(destFile);
}

void Snowflake::Client::FileTransferAgent::clearResults()
{
  for (auto it = executionResults.begin(); it != executionResults.end(); it++)
  {
    delete *it;
  }
}
