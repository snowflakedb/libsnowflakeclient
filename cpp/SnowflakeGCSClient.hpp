/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKEGCSCLIENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKEGCSCLIENT_HPP

#include "snowflake/IFileTransferAgent.hpp"
#include "IStorageClient.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include "FileMetadata.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include <map>

#ifdef _WIN32
 // see https://github.com/aws/aws-sdk-cpp/issues/402
#undef GetMessage
#undef GetObject
#undef min
#endif

namespace Snowflake
{
namespace Client
{

/**
 * Wrapper over Google Cloud Storage client
 */
class SnowflakeGCSClient : public Snowflake::Client::IStorageClient
{
public:
  SnowflakeGCSClient(StageInfo *stageInfo, unsigned int parallel,
                    TransferConfig * transferConfig,
                    IStatementPutGet* statement);

  ~SnowflakeGCSClient();

  /**
   * Upload data to bucket. Object metadata will be retrieved to
   * deduplicate file
   * @param fileMetadata
   * @param dataStream
   * @return
   */
  RemoteStorageRequestOutcome upload(FileMetadata *fileMetadata,
                         std::basic_iostream<char> *dataStream) override;

  RemoteStorageRequestOutcome download(FileMetadata *fileMetadata,
    std::basic_iostream<char>* dataStream) override;

  RemoteStorageRequestOutcome GetRemoteFileMetadata(
    std::string * filePathFull, FileMetadata *fileMetadata) override;

  virtual bool requirePresignedUrl() override
  {
    return true;
  }

private:
  /**
  * Add snowflake specific metadata to the put object metadata.
  * This includes encryption metadata and source file
  * message digest (after compression)
  * @param userMetadata
  * @param fileMetadata
  */
  void addUserMetadata(std::vector<std::string>& userMetadata,
    FileMetadata *fileMetadata);

  /*
  * buildEncryptionMetadataJSON
  * Takes the base64-encoded iv and key and creates the JSON block to be
  * used as the encryptiondata metadata field on the blob.
  */
  std::string buildEncryptionMetadataJSON(std::string key64, std::string iv64);

  void parseEncryptionMetadataFromJSON(std::string const& jsonStr,
                                       std::string& key64,
                                       std::string& iv64);

  void parseHttpRespHeaders(std::string const& headerString,
                            std::map<std::string, std::string>& headers);

  StageInfo * m_stageInfo;

  IStatementPutGet* m_statement;
};
}
}

#endif //SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
