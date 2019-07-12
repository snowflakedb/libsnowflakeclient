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
#define DATA_SIZE_THRESHOLD 5242880

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
  FileMetadataInitializer(std::vector<FileMetadata> *smallFileMetadata,
                          std::vector<FileMetadata> *largeFileMetadata);

  /**
   * Given a source locations, find all files that match the location pattern,
   * init file metadata, and divide them into different vector according to size
   */
  void populateSrcLocUploadMetadata(std::string &sourceLocation);

  /**
   * Given a source location, find out file size to determine use parallel
   * download or not.
   */
  RemoteStorageRequestOutcome populateSrcLocDownloadMetadata(
    std::string &sourceLocation, std::string *remoteLocations,
    IStorageClient *storageClient, EncryptionMaterial *encMat);

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

private:
  /**
   * Given file name, populate metadata
   * @param fileName
   */
  void initFileMetadata(std::string &fileDir, char *fileName, long fileSize);

  /**
   * init compression metadata
   */
  void initCompressionMetadata(FileMetadata *fileMetadata);

  /// small file metadata
  std::vector<FileMetadata> *m_smallFileMetadata;

  /// large file metadata
  std::vector<FileMetadata> *m_largeFileMetadata;

  /// auto compress
  bool m_autoCompress;

  /// source compression
  char *m_sourceCompression;

  /// encryption material
  std::vector<EncryptionMaterial> * m_encMat;
};
}
}

#endif //SNOWFLAKECLIENT_FILEPATTERNINTERPRETER_HPP
