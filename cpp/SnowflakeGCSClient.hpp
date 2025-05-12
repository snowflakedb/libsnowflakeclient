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
    return m_gcsAccessToken.empty();
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

  /**
  * build gcs request.
  * @param filePathFull the full path of the object (input)
  * @param url request url (output)
  * @param reqHeaders request headers (output)
  */
  void buildGcsRequest(const std::string &filePathFull,
                       std::string &url, std::vector<std::string>& reqHeaders);

  /**
  * Compose bucket and object value used for gcs request.
  * @param fileMetadata (input)
  * @param bucket (output)
  * @param object (output)
  */
  void extractBucketAndObject(const std::string &fileFullPath,
                              std::string &bucket, std::string &object);

  /**
  * Encode name to be a part of URL
  * @param srcName The source name to be encoded
  * @return The encode name
  */
  std::string encodeUrlName(const std::string &srcName);

  StageInfo * m_stageInfo;

  IStatementPutGet* m_statement;

  std::string m_gcsAccessToken;

  std::string m_stageEndpoint;
};
}
}

#endif //SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
