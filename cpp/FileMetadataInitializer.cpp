/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "FileMetadataInitializer.hpp"
#include "EncryptionProvider.hpp"
#include "logger/SFLogger.hpp"
#include <dirent.h>
#include <fnmatch.h>

#define COMPRESSION_AUTO "AUTO"
#define COMPRESSION_AUTO_DETECT "AUTO_DETECT"
#define COMPRESSION_NONE "NONE"

Snowflake::Client::FileMetadataInitializer::FileMetadataInitializer(
  std::vector<FileMetadata> * smallFileMetadata,
  std::vector<FileMetadata> * largeFileMetadata) :
  m_smallFileMetadata(smallFileMetadata),
  m_largeFileMetadata(largeFileMetadata),
  m_autoCompress(true)
{
}

void Snowflake::Client::FileMetadataInitializer::initFileMetadata(
  std::string &fileDir, char *fileName)
{
  std::string srcFileName = fileDir + fileName;
  struct stat fileStatus;
  if (!stat(srcFileName.c_str(), &fileStatus))
  {
    if (S_ISREG(fileStatus.st_mode))
    {
      std::vector<FileMetadata> *metaListToPush =
        fileStatus.st_size > DATA_SIZE_THRESHOLD ?
        m_largeFileMetadata : m_smallFileMetadata;

      metaListToPush->emplace_back();
      metaListToPush->back().srcFileName = srcFileName;
      metaListToPush->back().srcFileSize = (long)fileStatus.st_size;
      metaListToPush->back().destFileName = std::string(fileName);

      // process compression type
      initCompressionMetadata(&metaListToPush->back());
    }
  }
  else
  {
    CXX_LOG_ERROR("Cannot read path struct");
    throw;
  }
}

void Snowflake::Client::FileMetadataInitializer::populateSrcLocUploadMetadata(
  std::string &sourceLocation)
{
  unsigned long dirSep = sourceLocation.find_last_of('/');
  std::string dirPath = sourceLocation.substr(0, dirSep + 1);
  std::string filePattern = sourceLocation.substr(dirSep + 1);

  DIR * dir = nullptr;
  struct dirent * dir_entry;
  if ((dir = opendir(dirPath.c_str())) != NULL)
  {
    while((dir_entry = readdir(dir)) != NULL)
    {
      if (!fnmatch(filePattern.c_str(), dir_entry->d_name, 0))
      {
        initFileMetadata(dirPath, dir_entry->d_name);
      }
    }
    closedir(dir);
  }
  else
  {
    // open dir failed
    throw;
  }
}

void Snowflake::Client::FileMetadataInitializer::initCompressionMetadata(
  FileMetadata *fileMetadata)
{
  if(!strncasecmp(m_sourceCompression, COMPRESSION_AUTO_DETECT, 
                  sizeof(COMPRESSION_AUTO_DETECT)) ||
     !strncasecmp(m_sourceCompression, COMPRESSION_AUTO, 
                  sizeof(COMPRESSION_AUTO)))
  {
    // guess
    CXX_LOG_INFO("Auto detect on compression type");
    fileMetadata->sourceCompression = FileCompressionType::guessCompressionType(
      fileMetadata->srcFileName);
  }
  else if (!strncasecmp(m_sourceCompression, COMPRESSION_NONE, 
                        sizeof(COMPRESSION_NONE)))
  {
    CXX_LOG_INFO("No compression in source file");
    fileMetadata->sourceCompression = &FileCompressionType::NONE;
  }
  else
  {
    // look up
    fileMetadata->sourceCompression = FileCompressionType::lookUpByName(
      m_sourceCompression);

    if (!fileMetadata->sourceCompression)
    {
      // no compression found
      throw;
    }
  }

  if (fileMetadata->sourceCompression == &FileCompressionType::NONE)
  {
    fileMetadata->targetCompression = m_autoCompress ?
      &FileCompressionType::GZIP : &FileCompressionType::NONE;
    fileMetadata->requireCompress = m_autoCompress;
    fileMetadata->destFileName = m_autoCompress ?
      fileMetadata->destFileName + fileMetadata->targetCompression->
        getFileExtension() :
      fileMetadata->destFileName;
  }
  else
  {
    if (!fileMetadata->sourceCompression->getIsSupported())
    {
      throw;
    }

    fileMetadata->requireCompress = false;
    fileMetadata->targetCompression = fileMetadata->sourceCompression;
  }
}

void Snowflake::Client::FileMetadataInitializer::initEncryptionMetadata(
  FileMetadata *fileMetadata)
{
  EncryptionProvider::populateFileKeyAndIV(fileMetadata, &(m_encMat->at(0)));
  EncryptionProvider::encryptFileKey(fileMetadata, &(m_encMat->at(0)));
  EncryptionProvider::serializeEncMatDecriptor(fileMetadata, &(m_encMat->at(0)));

  // update encrypted stream size
  size_t encryptionBlockSize = Crypto::cryptoAlgoBlockSize(
    Crypto::CryptoAlgo::AES);

  fileMetadata->encryptionMetadata.cipherStreamSize = (long long int)
    ((fileMetadata->srcFileToUploadSize + encryptionBlockSize) /
    encryptionBlockSize * encryptionBlockSize);
  fileMetadata->destFileSize = fileMetadata->encryptionMetadata.cipherStreamSize;
}

Snowflake::Client::RemoteStorageRequestOutcome
Snowflake::Client::FileMetadataInitializer::
populateSrcLocDownloadMetadata(std::string &sourceLocation,
                               std::string *remoteLocation,
                               IStorageClient *storageClient,
                               EncryptionMaterial *encMat)
{
  std::string fullPath = *remoteLocation + sourceLocation;
  unsigned long dirSep = fullPath.find_last_of('/');
  std::string dstFileName = fullPath.substr(dirSep + 1);

  FileMetadata fileMetadata;
  RemoteStorageRequestOutcome outcome = storageClient->GetRemoteFileMetadata(
    &fullPath, &fileMetadata);

  if (outcome == RemoteStorageRequestOutcome::SUCCESS)
  {
    CXX_LOG_DEBUG("Success on getting remote file metadata");
    std::vector<FileMetadata> *metaListToPush =
      fileMetadata.srcFileSize > DATA_SIZE_THRESHOLD ?
      m_largeFileMetadata : m_smallFileMetadata;

    metaListToPush->push_back(fileMetadata);
    metaListToPush->back().srcFileName = fullPath;
    metaListToPush->back().destFileName = dstFileName;
    EncryptionProvider::decryptFileKey(&(metaListToPush->back()), encMat);
  }

  return outcome;
}


