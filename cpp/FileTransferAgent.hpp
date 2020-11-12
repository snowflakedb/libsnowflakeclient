/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
#define SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP

#include <map>
#include <string>
#include <snowflake/IFileTransferAgent.hpp>
#include "snowflake/IStatementPutGet.hpp"
#include "snowflake/IFileTransferAgent.hpp"
#include "snowflake/ITransferResult.hpp"
#include "FileTransferExecutionResult.hpp"
#include "FileMetadata.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/platform.h"
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <thread>

#endif

#define FILE_ENCRYPTION_BLOCK_SIZE 128

namespace Snowflake
{
namespace Client
{

class IStorageClient;

class FileTransferExecutionResult;

constexpr unsigned long MILLI_SECONDS_IN_SECOND = 1000;

class RetryContext
{
  public:
    RetryContext(const std::string &fileName, int maxRetries):
    m_retryCount(0),
    m_putFileName(fileName),
    m_maxRetryCount(maxRetries),
    m_minSleepTimeInMs(3 * MILLI_SECONDS_IN_SECOND), //3 seconds
    m_maxSleepTimeInMs(180 * MILLI_SECONDS_IN_SECOND), //180 seconds is the max sleep time
    m_timeoutInMs(maxRetries * 500 * MILLI_SECONDS_IN_SECOND) // timeout maxRetries * 500 seconds.
    {
        m_startTime = (unsigned long)time(NULL);
    }

    /**
     * It is retryable if put file status is failed
     * And retry count is in the limits
     * And total elapsed time is less than the timeout value specified.
     *
     * @param putStatus: Put upload status.
     * @return whether to retry or not.
     */
    bool isRetryable(RemoteStorageRequestOutcome putStatus)
    {
        //If putStatus is not SUCCESS and not TOKEN_EXPIRED then put is retryable
        bool isPutInRetryableState = ((putStatus != RemoteStorageRequestOutcome::SUCCESS) &&
                (putStatus != RemoteStorageRequestOutcome::TOKEN_EXPIRED)) ;
        //If file upload is successful in a retry log it
        if(putStatus == RemoteStorageRequestOutcome::SUCCESS && m_retryCount > 1)
        {
            CXX_LOG_DEBUG("After %d retry put %s successfully uploaded.", m_retryCount-1, m_putFileName.c_str());
        }
        unsigned long elapsedTime = time(NULL) - m_startTime;
        return isPutInRetryableState && m_retryCount <= m_maxRetryCount && elapsedTime < m_timeoutInMs;
    }

    /**
     * get's next sleep time and sleeps
     * sleep time in milli seconds.
     */
    void waitForNextRetry()
    {
        unsigned long sleepTime = retrySleepTimeInMs();
        if(sleepTime > 0) // Sleep only in the retries.
        {
#ifdef _WIN32
            Sleep(sleepTime);  // Sleep for sleepTime milli seconds (Sleep(<time in milliseconds>) in windows)
#else
            std::this_thread::sleep_for(std::chrono::milliseconds (std::chrono::milliseconds(sleepTime)));
#endif
            CXX_LOG_DEBUG("Retry count %d, Retrying after %ld milli seconds put file %s.", m_retryCount, sleepTime, m_putFileName.c_str());
        }
        ++m_retryCount;
    }

  private:
    unsigned long m_retryCount;
    unsigned long m_maxRetryCount;
    unsigned long m_minSleepTimeInMs;
    unsigned long m_maxSleepTimeInMs;
    unsigned long m_timeoutInMs;
    unsigned long m_startTime;
    std::string m_putFileName;
/**
 * When retryCount is 0 its the initial try for put and not a retry so return 0
 * XP’s backoff strategy (exponential backoff time with jitter).
 * start sleep time 3 second
 * max sleep time is 180 second,
 * Jitter factor is 0.5.
 * For example, the expected_sleep_time is 3, 6, 12, 24, etc
 * The sleep time is (expected_sleep_time/2 + a random number between [0, expected_sleep_time/2))
 * expected_sleep_time = 6
 * return's (6/2 + (rand() % 3) )
 * @return returns sleep time in milli seconds.
 */
    unsigned long retrySleepTimeInMs()
    {
        if(m_retryCount == 0 ) {
            return 0; //When its initial put (and not a retry)
        }

        unsigned long expectedSleepTimeInMs = m_minSleepTimeInMs * pow(2, (m_retryCount-1));

        expectedSleepTimeInMs = (std::min)(expectedSleepTimeInMs, m_maxSleepTimeInMs);

        unsigned long jitterInMs = (unsigned long)(rand() % (expectedSleepTimeInMs/2));

        expectedSleepTimeInMs = (unsigned long)((expectedSleepTimeInMs/2) + jitterInMs);

        return expectedSleepTimeInMs ;
    }
};

/**
 * This is the main class to external component (c api or ODBC)
 * External component should implement IStatement interface to submit put
 * or get command to server to do parsing.
 */
class FileTransferAgent : public IFileTransferAgent
{
public:
  FileTransferAgent(IStatementPutGet *statement,
                    TransferConfig * transferConfig = nullptr);

  ~FileTransferAgent();

  /**
   * Called by external component to execute put/get command
   * @param command put/get command
   * @return a fixed view result set representing upload/download result
   */
  virtual ITransferResult *execute(std::string *command);

  /**
  * Set upload stream to enable upload file from stream in memory.
  * @param uploadStream The stream to be uploaded.
  * @param dataSize The data size of the stream.
  */
  virtual void setUploadStream(std::basic_iostream<char>* uploadStream,
                               size_t dataSize)
  {
    m_uploadStream = uploadStream;
    m_uploadStreamSize = dataSize;
  }

  /**
   * Set useUrand to true to use /dev/urandom device
   * Set it to false to use /dev/random device
   * @param useUrand
   */
  virtual void setRandomDeviceAsUrand(bool useUrand)
  {
    m_useDevUrand = useUrand;
  }

  virtual void setPutFastFail(bool fastFail)
  {
    CXX_LOG_DEBUG("Setting fastFail to %d", fastFail);
    m_fastFail = fastFail;
  }

  virtual bool isPutFastFailEnabled(void)
  {
    return m_fastFail;
  }

  virtual void setPutMaxRetries(int maxRetries)
  {
     m_maxPutRetries = maxRetries;
  }

private:
  /**
   * Populate file metadata, (Get source file name)
   * Process compression metadata
   * Divide files to large and small ones
   */
  void initFileMetadata(std::string* command);

  /**
   * Upload large files in sequence, upload small files in parallel in
   * a thread pool object
   */
  void upload(std::string *command);

  /**
   * Upload single file.
   */
  RemoteStorageRequestOutcome uploadSingleFile(IStorageClient *client,
    FileMetadata *fileMetadata, size_t resultIndex);

  /**
   * Given file name, calculate sha256 message digest. The digest is
   * calculated after file is compressed (if needed)
   * @param fileName
   * @param digest
   */
  void updateFileDigest(FileMetadata *fileMetadata);

  /**
   * Renew aws expired token by re-submitting put/get command to server
   * @param client the new client object using the new token
   */
  void renewToken(std::string * command);

  void uploadFilesInParallel(std::string *command);

  /**
   * Download large files in sequence, download small files in parallel in
   * a thread pool object
   */
  void download(std::string *command);

  void downloadFilesInParallel(std::string *command);

  /**
   * Download single file.
   */
  RemoteStorageRequestOutcome downloadSingleFile(IStorageClient *client,
                                                 FileMetadata *fileMetadata, size_t resultIndex);

  /**
   * compress source file into a temporary file if required by
   * user.
   * @param fileMetadata
   */
  void compressSourceFile(FileMetadata *fileMetadata);

  /**
   * Reset private members between two consecutive put/get command
   */
  void reset();

  void getPresignedUrlForUploading(FileMetadata& io_fileMetadata,
                                   const std::string& command);

  /**
  * Parses out the local file path from the command. We need this to get the
  * file paths to expand wildcards and make sure the paths GS returns are
  * correct
  *
  * @param command  The GET/PUT command we send to GS
  * @param unescape True to unescape backslashes coming from GS
  * @return Path to the local file
  */
  std::string getLocalFilePathFromCommand(std::string const& command,
                                          bool unescape);

  /// interface to communicate with server
  IStatementPutGet *m_stmtPutGet;

  /// interface to communicate with remote storage
  IStorageClient * m_storageClient;

  /// Files that will be uploaded in sequence
  std::vector<FileMetadata> m_largeFilesMeta;

  /// Files that will be uploaded in parallel
  std::vector<FileMetadata> m_smallFilesMeta;

  /// vectors to store newly created execution result
  FileTransferExecutionResult *m_executionResults;

  /// parallel thread for upload/download small files
  PutGetParseResponse response;

  /// used to initialize file metadata
  FileMetadataInitializer m_FileMetadataInitializer;

  /// mutex to prevent from multiple thread renewing token same time
  SF_MUTEX_HANDLE m_parallelTokRenewMutex;

  /// mutex to prevent from multiple thread appending failed transfer file at the same time
  SF_MUTEX_HANDLE m_parallelFailedMsgMutex;

  /// seconds in unix time for last time token is refreshed
  long m_lastRefreshTokenSec;

  /// config struct that is passed in.
  TransferConfig * m_transferConfig;

  /// The stream for uploading data from memory. (NOT OWN)
  std::basic_iostream<char>* m_uploadStream;

  /// The data size of upload stream.
  size_t m_uploadStreamSize;

  /// Whether to use /dev/urandom or /dev/random;
  bool m_useDevUrand;

  /// fastFail, fail all the puts if one of the put fails in the wild char put upload.
  bool m_fastFail;

  int m_maxPutRetries;
};
}
}

#endif //SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
