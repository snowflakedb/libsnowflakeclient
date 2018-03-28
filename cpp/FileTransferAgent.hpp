//
// Created by hyu on 2/2/18.
//

#ifndef SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
#define SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP

#include <map>
#include <string>
#include "IStatementPutGet.hpp"
#include "FileTransferExecutionResult.hpp"
#include "FileMetadata.hpp"

namespace Snowflake
{
namespace Client
{
class StorageObjectMetadata;

class IStorageClient;

struct FileTransferExecutionResult;


/**
 * This is the main class to external component (c api or ODBC)
 * External component should implement IStatement interface to submit put
 * or get command to server to do parsing.
 */
class FileTransferAgent
{
public:
  FileTransferAgent(IStatementPutGet *statement);

  ~FileTransferAgent();

  /**
   * Called by external component to execute put/get command
   * @param command put/get command
   * @return a fixed view result set representing upload/download result
   */
  FileTransferExecutionResult *execute(std::string *command);

  std::vector<FileTransferExecutionResult *> *getResult()
  {
    return &executionResults;
  }

private:
  /**
   * Populate file metadata.
   * @param putGetParseResponse
   */
  void initFileMetadata(PutGetParseResponse *putGetParseResponse);

  /**
   * Upload large files in sequence, upload small files in parallel in
   * a thread pool object
   * @param stageInfo
   */
  void upload(StageInfo *stageInfo);

  /**
   * Upload single file.
   */
  FileTransferExecutionResult *uploadSingleFile(
    IStorageClient *client,
    FileMetadata *fileMetadata);

  /**
   * If non source compression, then compressed with gzip.
   * Otherwise determine compression type.
   * Also update FileMetadata.destFileName
   */
  void processCompressionType(FileMetadata *fileMetadata);

  /**
   * Given file name, calculate sha256 message digest. The digest is
   * calculated after file is compressed (if needed)
   * @param fileName
   * @param digest
   */
  void updateFileDigest(FileMetadata *fileMetadata);

  void download();

  /**
   * clean up results
   */
  void clearResults();

  /**
   * compress source file into a temporary file if required by
   * user.
   * @param fileMetadata
   */
  void compressSourceFile(FileMetadata *fileMetadata);

  IStatementPutGet *m_stmtPutGet;

  /// Files that will be uploaded in sequence
  std::map<std::string, FileMetadata> m_largeFilesMeta;

  /// Files that will be uploaded in parallel
  std::map<std::string, FileMetadata> m_smallFilesMeta;

  /// vectors to store newly created execution result
  std::vector<FileTransferExecutionResult *> executionResults;

  /// parallel thread for upload/download small files
  int parallel;
};
}
}

#endif //SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
