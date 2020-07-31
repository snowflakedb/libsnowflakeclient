/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_FILEPATTERNINTERPRETER_HPP
#define SNOWFLAKECLIENT_FILEPATTERNINTERPRETER_HPP

#include <string>
#include <vector>
#include "FileMetadata.hpp"
#include "IStorageClient.hpp"

// used to decide whether to upload in sequence or in parallel
#define DEFAULT_UPLOAD_DATA_SIZE_THRESHOLD 67108864 //64Mb
#define DOWNLOAD_DATA_SIZE_THRESHOLD 5242880 // 5MB

namespace Snowflake
{
namespace Client
{
/**
 * Class used to init various metadata.
 */
class FileMetadataInitializer
{
public:
  FileMetadataInitializer(std::vector<FileMetadata> &smallFileMetadata,
                          std::vector<FileMetadata> &largeFileMetadata);

  /**
   * Given a source locations, find all files that match the location pattern,
   * init file metadata, and divide them into different vector according to size
   */
  void populateSrcLocUploadMetadata(std::string &sourceLocation, size_t putThreshold);

  /**
   * Given a source location, find out file size to determine use parallel
   * download or not.
   */
  RemoteStorageRequestOutcome populateSrcLocDownloadMetadata(
    std::string &sourceLocation, std::string *remoteLocations,
    IStorageClient *storageClient, EncryptionMaterial *encMat,
    std::string const& presignedUrl);

  /**
   * Init encryption metadata in file metadata
   */
  void initEncryptionMetadata(FileMetadata *fileMetadata);

  inline void setAutoCompress(bool autoCompress)
  {
    m_autoCompress = autoCompress;
  }

  inline void setSourceCompression(char *sourceCompression)
  {
    m_sourceCompression = sourceCompression;
  }

  inline void setEncryptionMaterials(std::vector<EncryptionMaterial> *encMat)
  {
    m_encMat = encMat;
  }

  inline void setRandomDev(bool useUrand)
  {
    m_randDevice = (useUrand) ? Crypto::CryptoRandomDevice::DEV_URANDOM : Crypto::CryptoRandomDevice::DEV_RANDOM ;
  }

  Crypto::CryptoRandomDevice getRandomDev(void)
  {
    return m_randDevice;
  }

private:
  /**
   * Given file name, populate metadata
   * @param fileName
   */
  void initUploadFileMetadata(const std::string &fileDir, const char *fileName, long fileSize, size_t threshold);

  /**
   * init compression metadata
   */
  void initCompressionMetadata(FileMetadata &fileMetadata);

  /// small file metadata
  std::vector<FileMetadata> &m_smallFileMetadata;

  /// large file metadata
  std::vector<FileMetadata> &m_largeFileMetadata;

  /// auto compress
  bool m_autoCompress;

  /// source compression
  char *m_sourceCompression;

  /// encryption material
  std::vector<EncryptionMaterial> * m_encMat;

  /// Random device for crytpo random num generator.
  Crypto::CryptoRandomDevice m_randDevice;
};
}
}

#endif //SNOWFLAKECLIENT_FILEPATTERNINTERPRETER_HPP
