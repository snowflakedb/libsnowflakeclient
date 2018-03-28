//
// Created by hyu on 3/27/18.
//

#include "FileTransferExecutionResult.hpp"

#define STATUS_SKIPPED "SKIPPED"
#define STATUS_SUCCEED "SUCCEED"
#define MESSAGE_SKIPPED "File with same name and checksum already exists. SKIPPED"

namespace Snowflake
{
namespace Client
{

FileTransferExecutionResult::FileTransferExecutionResult(
  FileMetadata *fileMetadata,
  CommandType commandType,
  TransferOutcome outcome)
{
  switch (commandType)
  {
    case UPLOAD:
      switch (outcome)
      {
        case SUCCESS:
        case SKIPPED:
          source = fileMetadata->srcFileName.substr(
            fileMetadata->srcFileName.find_last_of('/') + 1);
          target = fileMetadata->destFileName;

          sourceSize = fileMetadata->srcFileSize;
          targetSize =
            outcome == SUCCESS ? fileMetadata->srcFileToUploadSize : 0;
          status = outcome == SUCCESS ? STATUS_SUCCEED : STATUS_SKIPPED;
          message = outcome == SUCCESS ? "" : MESSAGE_SKIPPED;

          //TODO update compression result
          break;

        default:
          throw;
      }

      break;

    case DOWNLOAD:
      break;

    default:
      throw;

  }
}

}
}