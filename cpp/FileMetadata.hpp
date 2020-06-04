/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_FILEMETADATA_HPP
#define SNOWFLAKECLIENT_FILEMETADATA_HPP

#include <string>
#include "snowflake/PutGetParseResponse.hpp"
#include "crypto/CryptoTypes.hpp"
#include "FileCompressionType.hpp"
#include <chrono>
#include "logger/SFLogger.hpp"

namespace Snowflake
{
namespace Client
{

struct EncryptionMetadata
{
  /// File encryption/decryption key
  Snowflake::Client::Crypto::CryptoIV iv;

  /// File key
  Snowflake::Client::Crypto::CryptoKey fileKey;

  /// base 64 encoded of encrypted file key
  std::string enKekEncoded;

  /// Encryption material descriptor
  std::string matDesc;

  /// encrypted stream size, used for content length
  long long int cipherStreamSize;
};

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

  /// Absolute path to the destination (including the filename. /tmp/small_test_file.csv.gz)
  std::string destPath;

  /// true if require gzip compression
  bool requireCompress;

  /// Upload and overwrite if file exists
  bool overWrite;

  /// encryption metadata
  EncryptionMetadata encryptionMetadata;

  /// file message digest (after compression if required)
  std::string sha256Digest;

  /// source compression 
  const FileCompressionType * sourceCompression;
  
  /// target compression
  const FileCompressionType * targetCompression;

  /// pre-signed url
  std::string presignedUrl;

  /// To estimate put/get performance.
  std::chrono::steady_clock::time_point startTime;

  FileMetadata(){
    startTime = std::chrono::steady_clock::now();
  }
  inline void printPutGetTimestamp(const char *debugmsg, bool rate=false)
  {
    auto curTime = std::chrono::steady_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(curTime - startTime).count();
    CXX_LOG_DEBUG("PutGetTimeStamp %s, elapsed time:%ld milli seconds, srcFilename:%s, srcFileSize:%ld.",
      debugmsg, timeDiff, srcFileName.c_str(), srcFileSize );
    if(rate)
    {
      long speed = ((srcFileSize*1000)/(timeDiff*1024)) ; //Speed in KiloBytes/Seconds
      CXX_LOG_DEBUG("PutGetTimeStamp: Rate of transfer is approximately %ld KiloBytes/Seconds.", speed);
    }
  }

};

}
}
#endif //SNOWFLAKECLIENT_FILEMETADATA_HPP
