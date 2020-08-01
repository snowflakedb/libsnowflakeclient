/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "FileMetadataInitializer.hpp"
#include "EncryptionProvider.hpp"
#include "logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "snowflake/SnowflakeTransferException.hpp"
#include <cerrno>

#define COMPRESSION_AUTO "AUTO"
#define COMPRESSION_AUTO_DETECT "AUTO_DETECT"
#define COMPRESSION_NONE "NONE"

#ifdef _WIN32
#include "Shlwapi.h"
#else
#include <dirent.h>
#include <fnmatch.h>
#endif



Snowflake::Client::FileMetadataInitializer::FileMetadataInitializer(
  std::vector<FileMetadata> &smallFileMetadata,
  std::vector<FileMetadata> &largeFileMetadata) :
  m_smallFileMetadata(smallFileMetadata),
  m_largeFileMetadata(largeFileMetadata),
  m_autoCompress(true)
{
}

void
Snowflake::Client::FileMetadataInitializer::initUploadFileMetadata(const std::string &fileDir, const char *fileName,
                                                                   long fileSize, size_t threshold)
{
  std::string fileNameFull = fileDir;
  fileNameFull += fileName;

  FileMetadata fileMetadata;
  fileMetadata.srcFileName = fileNameFull;
  fileMetadata.srcFileSize = fileSize;
  fileMetadata.destFileName = std::string(fileName);
  // process compression type
  initCompressionMetadata(fileMetadata);

  std::vector<FileMetadata> &metaListToPush = fileSize > threshold ?
    m_largeFileMetadata : m_smallFileMetadata;

  metaListToPush.push_back(fileMetadata);
}

void Snowflake::Client::FileMetadataInitializer::populateSrcLocUploadMetadata(std::string &sourceLocation,
                                                                              size_t putThreshold)
{
// looking for files on disk. 
#ifdef _WIN32
  WIN32_FIND_DATA fdd;
  HANDLE hFind = FindFirstFile(sourceLocation.c_str(), &fdd);
  if (hFind == INVALID_HANDLE_VALUE)
  {
    DWORD dwError = GetLastError();
    if (dwError == ERROR_NO_MORE_FILES || dwError == ERROR_FILE_NOT_FOUND)
    {
      CXX_LOG_ERROR("No file matching pattern %s has been found. Error: %d", 
        sourceLocation.c_str(), dwError);
      FindClose(hFind);
      return;
    }
    else if (dwError != ERROR_SUCCESS)
    {
      CXX_LOG_ERROR("Failed on FindFirstFile. Error: %d", dwError);
      throw SnowflakeTransferException(TransferError::DIR_OPEN_ERROR,
        sourceLocation.c_str(), dwError);
    }
  }

  do {
    if (!(fdd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
    {
      std::string fileFullPath = std::string(fdd.cFileName);
      size_t dirSep = sourceLocation.find_last_of(PATH_SEP);
      if (dirSep == std::string::npos) 
      {
        dirSep = sourceLocation.find_last_of(ALTER_PATH_SEP);
      }
      if (dirSep != std::string::npos) 
      {
        std::string dirPath = sourceLocation.substr(0, dirSep + 1);
        LARGE_INTEGER fileSize;
        fileSize.LowPart = fdd.nFileSizeLow;
        fileSize.HighPart = fdd.nFileSizeHigh;
        initUploadFileMetadata(dirPath, (char *)fdd.cFileName, (long)fileSize.QuadPart, putThreshold);
      }
    }
  } while (FindNextFile(hFind, &fdd) != 0);

  DWORD dwError = GetLastError();
  if (dwError != ERROR_NO_MORE_FILES)
  {
    CXX_LOG_ERROR("Failed on FindNextFile. Error: %d", dwError);
    throw SnowflakeTransferException(TransferError::DIR_OPEN_ERROR,
      sourceLocation.c_str(), dwError);
  }
  FindClose(hFind);

#else
  unsigned long dirSep = sourceLocation.find_last_of(PATH_SEP);
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
        std::string srcFileName = dirPath + dir_entry->d_name;
        struct stat fileStatus;
        int ret = stat(srcFileName.c_str(), &fileStatus);
        if (!ret)
        {
          if (S_ISREG(fileStatus.st_mode)) {
            initUploadFileMetadata(dirPath, dir_entry->d_name,
                                   (long) fileStatus.st_size, putThreshold);
          }
        }
        else
        {
          CXX_LOG_ERROR("Cannot read path struct");
          throw SnowflakeTransferException(TransferError::DIR_OPEN_ERROR,
                                           sourceLocation.c_str(), ret);
        }
      }
    }
    closedir(dir);
  }
  else
  {
    // open dir failed
    CXX_LOG_ERROR("Cannot open directory %s, errno(%d)",
      dirPath.c_str(), errno);
    throw SnowflakeTransferException(TransferError::DIR_OPEN_ERROR,
                                     dirPath.c_str(), errno);
  }
#endif
}

void Snowflake::Client::FileMetadataInitializer::initCompressionMetadata(
  FileMetadata &fileMetadata)
{
  CXX_LOG_DEBUG("Init compression metadata for file %s",
                fileMetadata.srcFileName.c_str());

  if(!sf_strncasecmp(m_sourceCompression, COMPRESSION_AUTO_DETECT, 
                  sizeof(COMPRESSION_AUTO_DETECT)) ||
     !sf_strncasecmp(m_sourceCompression, COMPRESSION_AUTO, 
                  sizeof(COMPRESSION_AUTO)))
  {
    // guess
    CXX_LOG_INFO("Auto detect on compression type");
    fileMetadata.sourceCompression = FileCompressionType::guessCompressionType(
      fileMetadata.srcFileName);
  }
  else if (!sf_strncasecmp(m_sourceCompression, COMPRESSION_NONE, 
                        sizeof(COMPRESSION_NONE)))
  {
    CXX_LOG_INFO("No compression in source file");
    fileMetadata.sourceCompression = &FileCompressionType::NONE;
  }
  else
  {
    // look up
    CXX_LOG_INFO("Compression type lookup by name.");
    fileMetadata.sourceCompression = FileCompressionType::lookUpByName(
      m_sourceCompression);

    if (!fileMetadata.sourceCompression)
    {
      // no compression found
      CXX_LOG_INFO("Compression type %s not found.", m_sourceCompression);
      throw SnowflakeTransferException(TransferError::COMPRESSION_NOT_SUPPORTED,
        m_sourceCompression);
    }
  }

  if (fileMetadata.sourceCompression == &FileCompressionType::NONE)
  {
    fileMetadata.targetCompression = m_autoCompress ?
      &FileCompressionType::GZIP : &FileCompressionType::NONE;
    fileMetadata.requireCompress = m_autoCompress;
    fileMetadata.destFileName = m_autoCompress ?
      fileMetadata.destFileName + fileMetadata.targetCompression->
        getFileExtension() :
      fileMetadata.destFileName;
  }
  else
  {
    if (!fileMetadata.sourceCompression->getIsSupported())
    {
      throw;
    }

    fileMetadata.requireCompress = false;
    fileMetadata.targetCompression = fileMetadata.sourceCompression;
  }
}

void Snowflake::Client::FileMetadataInitializer::initEncryptionMetadata(
  FileMetadata *fileMetadata)
{
  std::string randDev = (getRandomDev() == Crypto::CryptoRandomDevice::DEV_RANDOM)? "DEV_RANDOM" : "DEV_URANDOM";
  CXX_LOG_INFO("Snowflake::Client::FileMetadataInitializer::initEncryptionMetadata using random device %s.", randDev.c_str());
  EncryptionProvider::populateFileKeyAndIV(fileMetadata, &(m_encMat->at(0)), getRandomDev());
  EncryptionProvider::encryptFileKey(fileMetadata, &(m_encMat->at(0)), getRandomDev());
  EncryptionProvider::serializeEncMatDecriptor(fileMetadata, &(m_encMat->at(0)));

  // update encrypted stream size
  size_t encryptionBlockSize = Crypto::cryptoAlgoBlockSize(
    Crypto::CryptoAlgo::AES);

  fileMetadata->encryptionMetadata.cipherStreamSize = (long long int)
    ((fileMetadata->srcFileToUploadSize + encryptionBlockSize) /
    encryptionBlockSize * encryptionBlockSize);
  fileMetadata->destFileSize = (long)(fileMetadata->encryptionMetadata.cipherStreamSize);
}

Snowflake::Client::RemoteStorageRequestOutcome
Snowflake::Client::FileMetadataInitializer::
populateSrcLocDownloadMetadata(std::string &sourceLocation,
                               std::string *remoteLocation,
                               IStorageClient *storageClient,
                               EncryptionMaterial *encMat,
                               std::string const& presignedUrl)
{
  std::string fullPath = *remoteLocation + sourceLocation;
  size_t dirSep = fullPath.find_last_of('/');
  std::string dstFileName = fullPath.substr(dirSep + 1);

  FileMetadata fileMetadata;
  fileMetadata.presignedUrl = presignedUrl;
  RemoteStorageRequestOutcome outcome = storageClient->GetRemoteFileMetadata(
    &fullPath, &fileMetadata);

  if (outcome == RemoteStorageRequestOutcome::SUCCESS)
  {
    CXX_LOG_DEBUG("Success on getting remote file metadata");
    std::vector<FileMetadata> &metaListToPush =
      fileMetadata.srcFileSize > DOWNLOAD_DATA_SIZE_THRESHOLD ?
      m_largeFileMetadata : m_smallFileMetadata;

    metaListToPush.push_back(fileMetadata);
    metaListToPush.back().srcFileName = fullPath;
    metaListToPush.back().destFileName = dstFileName;
    EncryptionProvider::decryptFileKey(&(metaListToPush.back()), encMat, getRandomDev());
  }

  return outcome;
}


