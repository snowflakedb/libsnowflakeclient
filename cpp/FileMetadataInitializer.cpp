/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "FileMetadataInitializer.hpp"
#include "EncryptionProvider.hpp"
#include <dirent.h>
#include <fnmatch.h>

// used to decide whether to upload in sequence or in parallel
#define DATA_SIZE_THRESHOLD 5242880

Snowflake::Client::FileMetadataInitializer::FileMetadataInitializer(
  std::vector<FileMetadata> * smallFileMetadata,
  std::vector<FileMetadata> * largeFileMetadata) :
  m_smallFileMetadata(smallFileMetadata),
  m_largeFileMetadata(largeFileMetadata)
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
      /*std::vector<FileMetadata> *metaListToPush =
        m_largeFileMetadata;*/

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
    throw;
  }
}

void Snowflake::Client::FileMetadataInitializer::populateSrcLocMetadata(
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
  if(!strncasecmp(m_sourceCompression, "AUTO_DETECT", 11) ||
     !strncasecmp(m_sourceCompression, "AUTO", 4))
  {
    // guess
    fileMetadata->sourceCompression = FileCompressionType::guessCompressionType(
      fileMetadata->srcFileName);
  }
  else if (!strncasecmp(m_sourceCompression, "NONE", 4))
  {
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
    fileMetadata->targetCompression = &FileCompressionType::GZIP;
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
  EncryptionProvider::populateFileKeyAndIV(fileMetadata, m_encMat);
  EncryptionProvider::encryptFileKey(fileMetadata, m_encMat);
  EncryptionProvider::serializeEncMatDecriptor(fileMetadata, m_encMat);

  // update encrypted stream size
  size_t encryptionBlockSize = Crypto::cryptoAlgoBlockSize(
    Crypto::CryptoAlgo::AES);

  fileMetadata->encryptionMetadata.cipherStreamSize = (long long int)
    ((fileMetadata->srcFileToUploadSize + encryptionBlockSize) /
    encryptionBlockSize * encryptionBlockSize);
}




