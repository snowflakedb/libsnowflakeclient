#pragma once
#ifndef SNOWFLAKECLIENT_CLIENTBINDUPLOADER_HPP
#define SNOWFLAKECLIENT_CLIENTBINDUPLOADER_HPP

#include <chrono>
#include "snowflake/client.h"
#include "snowflake/BindUploader.hpp"

namespace Snowflake
{
namespace Client
{

class ClientBindUploader : public BindUploader
{
public:
  /**
  * constructor
  *
  * @param sfstmt The SNOWFLAKE_STMT context.
  * @param stageDir The unique stage path for bindings uploading, could be a GUID.
  * @param numParams Number of parameters.
  * @param numParamSets Number of parameter sets.
  * @param maxFileSize The max size of single file for bindings uploading.
  *                    Separate into multiple files when exceed.
  * @param compressLevel The compress level, between -1(default) to 9.
  */
  explicit ClientBindUploader(SF_STMT *sfstmt,
                              const std::string& stageDir,
                              unsigned int numParams,
                              unsigned int numParamSets,
                              unsigned int maxFileSize,
                              int compressLevel);

  ~ClientBindUploader();

protected:
  /**
  * Check whether the session's temporary stage has been created, and create it
  * if not.
  *
  * @Return true if succeeded, false otherwise.
  */
  virtual bool createStageIfNeeded() override;

  /**
  * Execute uploading for single data file.
  *
  * @param sql PUT command for single data file uploading
  * @param uploadStream stream for data file to be uploaded
  * @param dataSize Size of the data to be uploaded.
  *
  * @Return true if succeeded, false otherwise.
  */
  virtual bool executeUploading(const std::string &sql,
                                std::basic_iostream<char>& uploadStream,
                                size_t dataSize) override;

private:
  // SNOWFLAKE_STMT context
  SF_STMT * m_stmt;

};

} // namespace Client
} // namespace Snowflake

#endif  // SNOWFLAKECLIENT_CLIENTBINDUPLOADER_HPP
