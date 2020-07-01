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
  std::chrono::steady_clock::time_point tstamps[10] ;

  /// Enum for different timestamps
  enum putGetTimestamp{PUTGET_START=0,COMP_START, COMP_END, PUT_START, PUT_END, PUTGET_END, GET_START, GET_END};

  inline void recordPutGetTimestamp(putGetTimestamp ts=PUTGET_START)
  {
    tstamps[ts] = std::chrono::steady_clock::now();
  }

  FileMetadata() {
    recordPutGetTimestamp();
  }

  ~FileMetadata()
  {
	if (requireCompress && !srcFileToUpload.empty())
	{
	  sf_delete_uniq_dir_if_exists(srcFileToUpload.c_str());
	}
  }

  inline void printPutGetTimestamp(void)
  {
    auto compTime = std::chrono::duration_cast<std::chrono::milliseconds>(tstamps[COMP_END] - tstamps[COMP_START]).count();
    auto putTime = std::chrono::duration_cast<std::chrono::milliseconds>(tstamps[PUT_END] - tstamps[PUT_START]).count();
    auto putgetTime = std::chrono::duration_cast<std::chrono::milliseconds>(tstamps[PUTGET_END] - tstamps[PUTGET_START]).count();

    unsigned long fssize = (srcFileToUploadSize > 0)? srcFileToUploadSize : srcFileSize ;

    CXX_LOG_DEBUG("Time took for compression: %ld milli seconds, srcFilename:%s, srcFileSize:%ld.",
        compTime, srcFileName.c_str(), srcFileSize );
    CXX_LOG_DEBUG("Time took to upload + Encryption %ld bytes : %ld milli seconds.", fssize, putTime );
    //Speed in KiloBytes/Seconds SourceFilesize is in bytes and putTime is in milliseconds
    unsigned long speed;
    if(putTime > 0) {
      speed = ((fssize*1000) /(putTime*1024));
      CXX_LOG_INFO("Upload speed with encryption: %ld kilobytes/sec.", speed);
    }
    if(compTime > 0 || putTime > 0 ) {
      speed = ((fssize*1000) / ((compTime + putTime)*1024));
      CXX_LOG_INFO("Upload speed with encryption and compression: %ld kilobytes/sec.", speed);
    }
    if(putgetTime > 0) {
      speed = ((fssize*1000) / (putgetTime*1024));
      CXX_LOG_DEBUG("Upload speed end to end: %ld kilobytes/sec.", speed);
    }
  }
};

}
}
#endif //SNOWFLAKECLIENT_FILEMETADATA_HPP
