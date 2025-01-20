/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

/*
 * Notice: For Snowflake internal use only.
 *         External application should not use this class.
 */

#pragma once
#ifndef SNOWFLAKECLIENT_BINDUPLOADER_HPP
#define SNOWFLAKECLIENT_BINDUPLOADER_HPP

#include <chrono>
#include <snowflake/Exceptions.hpp>
#include "client.h"

namespace Snowflake
{
namespace Client
{

class BindUploader
{
public:
  /**
  * constructor
  *
  * @param stageDir The unique stage path for bindings uploading, could be a GUID.
  * @param numParams Number of parameters.
  * @param numParamSets Number of parameter sets.
  * @param maxFileSize The max size of single file for bindings uploading.
  *                    Separate into multiple files when exceed.
  * @param compressLevel The compress level, between -1(default) to 9.
  */
  explicit BindUploader(const std::string& stageDir,
                        unsigned int numParams,
                        unsigned int numParamSets,
                        unsigned int maxFileSize,
                        int compressLevel);

  void addStringValue(const std::string& value, SF_DB_TYPE type);

  void addNullValue();

  inline std::string getStagePath()
  {
    return m_stagePath;
  }

  inline bool hasBindingUploaded()
  {
    return m_hasBindingUploaded;
  }

protected:
  /**
  * @return The statement for creating temporary stage for bind uploading.
  */
  std::string getCreateStageStmt();

  /**
  * Check whether the session's temporary stage has been created, and create it
  * if not.
  *
  * @throws Exception if creating the stage fails
  */
  virtual void createStageIfNeeded() = 0;

  /**
  * Execute uploading for single data file.
  *
  * @param sql PUT command for single data file uploading
  * @param uploadStream stream for data file to be uploaded
  * @param dataSize Size of the data to be uploaded.
  *
  * @throws Exception if uploading fails
  */
  virtual void executeUploading(const std::string &sql,
                                std::basic_iostream<char>& uploadStream,
                                size_t dataSize) = 0;

  /* date/time format conversions to be overridden by drivers (such as ODBC)
   * that need native date/time type support.
   * Will be called to converting binding format between regular binding and
   * bulk binding.
   * No conversion by default, in such case application/driver should bind
   * data/time data as string.
   */

  /**
  * Convert time data format from nanoseconds to HH:MM:SS.F9
  * @param timeInNano The time data string in nanoseconds.
  */
  virtual std::string convertTimeFormat(const std::string& timeInNano)
  {
    return timeInNano;
  }

  /**
  * Convert date data format from days to YYYY-MM-DD
  * @param milliseconds since Epoch
  */
  virtual std::string convertDateFormat(const std::string& millisecondSinceEpoch)
  {
    return millisecondSinceEpoch;
  }

  /**
  * Convert timestamp data format from nanoseconds to YYYY_MM_DD HH:MM:SS.F9
  * @param timestampInNano The timestamp data string in nanoseconds.
  * @param type Either TIMESTAMP_LTZ or NTZ depends on CLIENT_TIMESTAMP_TYPE_MAPPING
  */
  virtual std::string convertTimestampFormat(const std::string& timestampInNano,
                                     SF_DB_TYPE type)
  {
    return timestampInNano;
  }

  /**
  * Revert time data format from HH:MM:SS.F9 to nanoseconds
  * @param formatedTime The time data string in HH:MM:SS.F9.
  */
  virtual std::string revertTimeFormat(const std::string& formatedTime)
  {
    return formatedTime;
  }

  /**
  * Convert date data format from YYYY-MM-DD to milliseconds since Epoch
  * @param formatedDate the date string in YYYY-MM-DD
  */
  virtual std::string revertDateFormat(const std::string& formatedDate)
  {
    return formatedDate;
  }

  /**
  * Convert timestamp data format from YYYY_MM_DD HH:MM:SS.F9 to nanoseconds
  * @param Formatedtimestamp The timestamp data string in YYYY_MM_DD HH:MM:SS.F9.
  * @param type Either TIMESTAMP_LTZ or NTZ depends on CLIENT_TIMESTAMP_TYPE_MAPPING
  */
  virtual std::string revertTimestampFormat(const std::string& Formatedtimestamp,
                                    SF_DB_TYPE type)
  {
    return Formatedtimestamp;
  }

private:
  /**
  * Upload serialized binds in CSV stream to stage
  *
  * @throws BindException if uploading the binds fails
  */
  void putBinds();

  /**
  * Compress data from csv stream to compress stream with gzip
  * @return The data size of compress stream if compress succeeded.
  * @throw when compress failed.
  */
  size_t compressWithGzip();

  /**
  * Build PUT statement string. Handle filesystem differences and escaping backslashes.
  * @param srcFilePath The faked source file path to upload.
  */
  std::string getPutStmt(const std::string& srcFilePath);

  /**
  * csv parsing function called by convertBindingFromCsvToJson(), get value of
  * next field.
  * @param fieldValue The output of the field value.
  * @param isNull The output of the flag whether the filed is null.
  * @param isEndofRow The output of the flag wether the end of row is reached.
  * @return true if a field value is retrieved successfully, false if end of data
  *         is reached and no field value available.
  */
  bool csvGetNextField(std::string& fieldValue, bool& isNull, bool& isEndofRow);

  std::stringstream m_csvStream;

  std::stringstream m_compressStream;

  std::string m_stagePath;

  unsigned int m_fileNo;

  unsigned int m_retryCount;

  unsigned int m_maxFileSize;

  unsigned int m_numParams;

  unsigned int m_numParamSets;

  unsigned int m_curParamIndex;

  unsigned int m_curParamSetIndex;

  size_t m_dataSize;

  std::chrono::steady_clock::time_point m_startTime;

  std::chrono::steady_clock::time_point m_serializeStartTime;

  long long m_compressTime;

  long long m_serializeTime;

  long long m_putTime;

  bool m_hasBindingUploaded;

  int m_compressLevel;

};

} // namespace Client
} // namespace Snowflake

#endif  // SNOWFLAKECLIENT_BINDUPLOADER_HPP
