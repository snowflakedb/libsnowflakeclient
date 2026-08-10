#ifndef SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP

#include "snowflake/IFileTransferAgent.hpp"
#include "IStorageClient.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include "FileMetadata.hpp"
#include "util/ThreadPool.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include "azure/storage/blobs.hpp"
#include <memory>
#include <sstream>
#include <string>

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
 * Context passed to upload thread
 */

struct MultiUploadCtx_a
{
  MultiUploadCtx_a(unsigned int partNumber,
                   std::string &uploadId)
    : buf(NULL),
      m_partNumber(partNumber),
      m_uploadId(uploadId),
      m_outcome(RemoteStorageRequestOutcome::FAILED)
  {

  }

  /// in memory buffer used to store current part data
  Util::ByteArrayStreamBuf *buf;

  /// part number
  unsigned int m_partNumber;

  /// uploadId
  std::string m_uploadId;

  /// upload outcome
  RemoteStorageRequestOutcome m_outcome;
};

struct MultiDownloadCtx_a
{
  /// in memory buffer used to store current part data
  Util::ByteArrayStreamBuf *buf;

  ///Start byte of the chunk.
  long long startbyte;

  /// part number
  unsigned int m_partNumber;

  /// upload outcome
  RemoteStorageRequestOutcome m_outcome;
};

class AzureStreamAdapter : public Azure::Core::IO::BodyStream
{
public:
  explicit AzureStreamAdapter(std::iostream& stream, int64_t length) :
    m_stream(stream), m_length(length) {}

private:
  std::iostream& m_stream;
  int64_t m_length;

  size_t OnRead(uint8_t* buffer, size_t count, Azure::Core::Context const& /*context*/) override
  {
    this->m_stream.read(reinterpret_cast<char*>(buffer), count);
    return m_stream.gcount();
  }

  int64_t Length() const override
  {
    return m_length;
  }

  void Rewind() override
  {
    ;// Do nothing
    /* The data stream could be Crypto::CipherIOStream which doesn't allow reset.
     * We've retried on FileTransferAgent level.
     */
  }
};

/**
 * Wrapper over Azure client
 */
class SnowflakeAzureClient : public Snowflake::Client::IStorageClient
{
public:
  SnowflakeAzureClient(StageInfo *stageInfo, unsigned int parallel, size_t uploadThreshold,
                       TransferConfig *transferConfig, IStatementPutGet* statement,
                       unsigned int maxRetries);

  ~SnowflakeAzureClient();

  /**
   * Upload data to Azure container. Object metadata will be retrieved to
   * deduplicate file
   * @param fileMetadata
   * @param dataStream
   * @return
   */
  RemoteStorageRequestOutcome upload(FileMetadata *fileMetadata,
                         std::basic_iostream<char> *dataStream);

  RemoteStorageRequestOutcome download(FileMetadata *fileMetadata,
    std::basic_iostream<char>* dataStream);

  RemoteStorageRequestOutcome doSingleDownload(FileMetadata *fileMetadata,
    std::basic_iostream<char>* dataStream);

  RemoteStorageRequestOutcome doMultiPartDownload(FileMetadata * fileMetadata,
    std::basic_iostream<char> *dataStream);

  RemoteStorageRequestOutcome GetRemoteFileMetadata(
    std::string * filePathFull, FileMetadata *fileMetadata);

private:


  StageInfo * m_stageInfo;

  Util::ThreadPool * m_threadPool;
  std::shared_ptr<Azure::Storage::Blobs::BlobServiceClient> m_blobServiceClient;
  // client with retry disabled for single uploading
  std::shared_ptr<Azure::Storage::Blobs::BlobServiceClient> m_blobServiceClientNoRetry;

  const size_t m_uploadThreshold;
  unsigned int m_parallel;

  /**
   * Max retries for multipart upload
   */
  unsigned int m_maxRetries;

  /**
   * Add snowflake specific metadata to the put object metadata.
   * This includes encryption metadata and source file
   * message digest (after compression)
   * @param userMetadata
   * @param fileMetadata
   */
  void addUserMetadata(Azure::Storage::Metadata &userMetadata,
                       FileMetadata *fileMetadata);

  /**
   * Compose bucket and key value used for s3 request.
   * @param fileMetadata
   * @param bucket
   * @param key
   */
  void extractBucketAndKey(std::string *fileFullPath, std::string &bucket,
                           std::string &key);

  RemoteStorageRequestOutcome doSingleUpload(FileMetadata * fileMetadata,
                                 std::basic_iostream<char> *dataStream);

  RemoteStorageRequestOutcome doMultiPartUpload(FileMetadata * fileMetadata,
                                    std::basic_iostream<char> *dataStream);

  void uploadParts(Azure::Storage::Blobs::BlockBlobClient* blobClient,
                   MultiUploadCtx_a* uploadCtx);

  void setMaxRetries(unsigned int maxRetries);
};
}
}

#endif //SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
