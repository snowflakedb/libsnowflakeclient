/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_FILEMETADATA_HPP
#define SNOWFLAKECLIENT_FILEMETADATA_HPP

#include <string>
#include "EncryptionMaterial.hpp"
#include "StageInfo.hpp"
#include "PutGetParseResponse.hpp"
#include "FileCompressionType.hpp"

namespace Snowflake
{
namespace Client
{
/**
 * File metadata used when doing upload/download
 */
struct FileMetadata
{
  /// original source file name (full path)
  std::string srcFileName;

  /// original source file size
  long srcFileSize;

  /// Temp file if compressed is required, otherwise same as src file
  std::string srcFileToUpload;

  /// Temp file size if compressed is required, otherwise same as src file
  long srcFileToUploadSize;

  /// destination file name (no path)
  std::string destFileName;

  /// destination file size
  long destFileSize;

  /// true if require gzip compression
  bool requireCompress;

  /// encryption metadata
  EncryptionMetadata encryptionMetadata;

  /// file message digest (after compression if required)
  std::string sha256Digest;

  /// source compression 
  const FileCompressionType * sourceCompression;
  
  /// target compression
  const FileCompressionType * targetCompression;
};
}
}
#endif //SNOWFLAKECLIENT_FILEMETADATA_HPP
