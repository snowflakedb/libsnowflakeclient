/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "FileMetadataInitializer.hpp"
#include "EncryptionProvider.hpp"
#include "logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "snowflake/SnowflakeTransferException.hpp"
#include <cerrno>
#include "boost/filesystem.hpp"

#define COMPRESSION_AUTO "AUTO"
#define COMPRESSION_AUTO_DETECT "AUTO_DETECT"
#define COMPRESSION_NONE "NONE"

#ifdef _WIN32
#include "Shlwapi.h"
#else
#include <dirent.h>
#include <fnmatch.h>
#endif

using namespace boost::filesystem;

Snowflake::Client::FileMetadataInitializer::FileMetadataInitializer(
  std::vector<FileMetadata> &smallFileMetadata,
  std::vector<FileMetadata> &largeFileMetadata,
  IStatementPutGet *stmtPutGet) :
  m_smallFileMetadata(smallFileMetadata),
  m_largeFileMetadata(largeFileMetadata),
  m_autoCompress(true),
  m_stmtPutGet(stmtPutGet)
{
}

void
Snowflake::Client::FileMetadataInitializer::initUploadFileMetadata(const std::string &fileNameFull,
                                                                   const std::string &destPath,
                                                                   const std::string &fileName,
                                                                   size_t fileSize, size_t threshold)
{
  FileMetadata fileMetadata;
  fileMetadata.srcFileName = m_stmtPutGet->platformStringToUTF8(fileNameFull);
  fileMetadata.srcFileSize = fileSize;
  fileMetadata.destPath = m_stmtPutGet->platformStringToUTF8(destPath);
  fileMetadata.destFileName = m_stmtPutGet->platformStringToUTF8(fileName);
  // process compression type
  initCompressionMetadata(fileMetadata);

  fileMetadata.isLarge = fileSize > threshold;
  std::vector<FileMetadata> &metaListToPush = fileMetadata.isLarge ?
    m_largeFileMetadata : m_smallFileMetadata;

  metaListToPush.push_back(fileMetadata);
}

void Snowflake::Client::FileMetadataInitializer::populateSrcLocUploadMetadata(std::string &sourceLocation,
                                                                              size_t putThreshold)
{
  // looking for files on disk.
  std::string srcLocationPlatform = m_stmtPutGet->UTF8ToPlatformString(sourceLocation);
  replaceStrAll(srcLocationPlatform, "/", std::string() + PATH_SEP);
  size_t dirSep = srcLocationPlatform.find_last_of(PATH_SEP);
  std::string basePath = srcLocationPlatform.substr(0, dirSep + 1);

  std::vector<std::string> fileList;
  if (!listFiles(srcLocationPlatform, fileList))
  {
    CXX_LOG_ERROR("Failed on finding files for uploading.");
    return;
  }

  for (auto file = fileList.begin(); file != fileList.end(); file++)
  {
    path p(*file);
    size_t fileSize = file_size(p);
    std::string fileNameFull = p.string();
    std::string fileName = p.filename().string();
    //make the path on stage by removing base path and file name from full path
    std::string destPath = fileNameFull.substr(basePath.length(),
                                               fileNameFull.length() - basePath.length() - fileName.length());
    initUploadFileMetadata(fileNameFull, destPath, fileName, fileSize, putThreshold);
  }
}

void Snowflake::Client::FileMetadataInitializer::includeSubfolderFilesRecursive(const std::string &folderPath,
                                                                                std::vector<std::string> & fileList)
{
  for (auto const& entry : recursive_directory_iterator(folderPath))
  {
    if (is_regular_file(entry))
    {
      fileList.push_back(entry.path().string());
    }
  }
}

bool Snowflake::Client::FileMetadataInitializer::listFiles(const std::string &sourceLocation,
                                                                    std::vector<std::string> & fileList)
{
// looking for files on disk. 
  std::string srcLocationPlatform = m_stmtPutGet->UTF8ToPlatformString(sourceLocation);
  size_t dirSep = srcLocationPlatform.find_last_of(PATH_SEP);
  std::string dirPath = srcLocationPlatform.substr(0, dirSep + 1);
  std::string filePattern = srcLocationPlatform.substr(dirSep + 1);
  bool includeSubfolder = filePattern == "**";

#ifdef _WIN32
  WIN32_FIND_DATA fdd;
  HANDLE hFind = FindFirstFile(srcLocationPlatform.c_str(), &fdd);
  if (hFind == INVALID_HANDLE_VALUE)
  {
    DWORD dwError = GetLastError();
    if (dwError == ERROR_NO_MORE_FILES || dwError == ERROR_FILE_NOT_FOUND ||
        dwError == ERROR_PATH_NOT_FOUND)
    {
      CXX_LOG_ERROR("No file matching pattern %s has been found. Error: %d", 
        sourceLocation.c_str(), dwError);
      return false;
    }
    else if (dwError != ERROR_SUCCESS)
    {
      CXX_LOG_ERROR("Failed on FindFirstFile. Error: %d", dwError);
      throw SnowflakeTransferException(TransferError::DIR_OPEN_ERROR,
        srcLocationPlatform.c_str(), dwError);
    }
  }

  do {
    if (!(fdd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
    {
      fileList.push_back(dirPath + fdd.cFileName);
    }
    else
    {
      if (includeSubfolder &&
          (std::string(fdd.cFileName) != ".") &&
          (std::string(fdd.cFileName) != ".."))
      {
        includeSubfolderFilesRecursive(dirPath + fdd.cFileName, fileList);
      }
    }
  } while (FindNextFile(hFind, &fdd) != 0);

  DWORD dwError = GetLastError();
  FindClose(hFind);
  if (dwError != ERROR_NO_MORE_FILES)
  {
    CXX_LOG_ERROR("Failed on FindNextFile. Error: %d", dwError);
    throw SnowflakeTransferException(TransferError::DIR_OPEN_ERROR,
      srcLocationPlatform.c_str(), dwError);
  }

#else
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
            fileList.push_back(dirPath + dir_entry->d_name);
          }
          else if (includeSubfolder &&
                   (S_ISDIR(fileStatus.st_mode)) &&
                   (std::string(dir_entry->d_name) != ".") &&
                   (std::string(dir_entry->d_name) != ".."))
          {
            includeSubfolderFilesRecursive(dirPath + dir_entry->d_name, fileList);
          }
        }
        else
        {
          CXX_LOG_ERROR("Cannot read path struct");
          throw SnowflakeTransferException(TransferError::DIR_OPEN_ERROR,
                                           srcLocationPlatform.c_str(), ret);
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
  return true;
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
    std::string srcFileNamePlatform = m_stmtPutGet->UTF8ToPlatformString(
      fileMetadata.srcFileName);
    fileMetadata.sourceCompression = FileCompressionType::guessCompressionType(
      srcFileNamePlatform);
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
  if (m_encMat->empty())
  {
    // No encryption materials for server side encryption
    fileMetadata->encryptionMetadata.cipherStreamSize = (long long)fileMetadata->srcFileToUploadSize;
    fileMetadata->destFileSize = fileMetadata->srcFileToUploadSize;
    fileMetadata->encryptionMetadata.fileKey.nbBits = 0;
    return;
  }

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
  fileMetadata->destFileSize = (size_t)(fileMetadata->encryptionMetadata.cipherStreamSize);
}

Snowflake::Client::RemoteStorageRequestOutcome
Snowflake::Client::FileMetadataInitializer::
populateSrcLocDownloadMetadata(std::string &sourceLocation,
                               std::string *remoteLocation,
                               IStorageClient *storageClient,
                               EncryptionMaterial *encMat,
                               std::string const& presignedUrl,
                               size_t getThreshold)
{
  std::string fullPath = *remoteLocation + sourceLocation;
  size_t dirSep = sourceLocation.find_last_of('/');
  std::string dstFileName = sourceLocation.substr(dirSep + 1);
  std::string dstPath = sourceLocation.substr(0, dirSep + 1);

  FileMetadata fileMetadata;
  fileMetadata.presignedUrl = presignedUrl;
  RemoteStorageRequestOutcome outcome = storageClient->GetRemoteFileMetadata(
    &fullPath, &fileMetadata);

  if (outcome == RemoteStorageRequestOutcome::SUCCESS)
  {
    CXX_LOG_DEBUG("Success on getting remote file metadata");
    fileMetadata.isLarge = fileMetadata.srcFileSize > getThreshold;
    std::vector<FileMetadata> &metaListToPush = fileMetadata.isLarge ?
      m_largeFileMetadata : m_smallFileMetadata;

    metaListToPush.push_back(fileMetadata);
    metaListToPush.back().srcFileName = fullPath;
    metaListToPush.back().destFileName = dstFileName;
    metaListToPush.back().destPath = dstPath;
    if (encMat)
    {
      EncryptionProvider::decryptFileKey(&(metaListToPush.back()), encMat, getRandomDev());
    }
    else
    {
      metaListToPush.back().encryptionMetadata.fileKey.nbBits = 0;
    }
  }

  return outcome;
}

void Snowflake::Client::FileMetadataInitializer::
replaceStrAll(std::string& stringToReplace,
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

