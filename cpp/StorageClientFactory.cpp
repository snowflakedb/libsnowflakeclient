#include "snowflake/SnowflakeTransferException.hpp"
#include "StorageClientFactory.hpp"
#include "SnowflakeS3Client.hpp"
#include "SnowflakeAzureClient.hpp"
#include "SnowflakeGCSClient.hpp"
#include "logger/SFLogger.hpp"

namespace Snowflake
{
namespace Client
{

IStorageClient * StorageClientFactory::injectedClient = nullptr;

IStorageClient * StorageClientFactory::getClient(StageInfo *stageInfo,
                                                 unsigned int parallel,
                                                 size_t uploadThreshold,
                                                 TransferConfig *transferConfig,
                                                 IStatementPutGet * statement,
                                                 unsigned int maxPutRetries)
{
  IStorageClient * client = NULL;
  switch (stageInfo->stageType)
  {
    case StageType::S3:
      CXX_LOG_INFO("Creating S3 client");
      client =  new SnowflakeS3Client(stageInfo, parallel, uploadThreshold, transferConfig, statement);
    case StageType::MOCKED_STAGE_TYPE:
      client = injectedClient;
    case StageType::AZURE:
      CXX_LOG_INFO("Creating Azure client");
      client = new SnowflakeAzureClient(stageInfo, parallel, uploadThreshold,
                                      transferConfig, statement, maxPutRetries);
    case StageType::GCS:
      CXX_LOG_INFO("Creating GCS client");
      client = new SnowflakeGCSClient(stageInfo, parallel, transferConfig, statement);
    default:
      // invalid stage type
      throw SnowflakeTransferException(TransferError::UNSUPPORTED_FEATURE,
        "Remote storage not supported.");
  }
  client->setMaxRetries(maxPutRetries);
}

void StorageClientFactory::injectMockedClient(IStorageClient *client)
{
  injectedClient = client;
}

}
}
