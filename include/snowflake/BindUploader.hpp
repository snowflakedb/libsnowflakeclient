/*
* File:   BindUploader.hpp
* Author: harryx
*
* Copyright (c) 2020 Snowflake Computing
*
* Created on March 5, 2020, 3:14 PM
*/

#pragma once
#ifndef BINDUPLOADER_HPP
#define BINDUPLOADER_HPP

#include "picojson.h"
#include "Statement.hpp"
#include "Logger.hpp"

namespace sf
{
  using namespace picojson;

  /**
  * Class BindUploader
  */
  class BindUploader
  {
  public:
    explicit BindUploader(Connection &connection,
                          const simba_wstring& stageDir,
                          unsigned int numParams,
                          unsigned int numParamSets,
                          int compressLevel,
                          bool injectError);

    void addStringValue(const std::string& value, simba_int16 type);

    void addNullValue();

    inline std::string getStagePath()
    {
      return m_stagePath.GetAsUTF8();
    }

    /**
    * Convert binding data in csv format for stage binding into json format
    * for regular binding. This is for fallback to regular binding when stage
    * binding fails.
    * @param paramBindOrder The bind order for parameters with parameter names.
    * @param parameterBinds The output of parameter bindings in json
    */
    void convertBindingFromCsvToJson(std::vector<std::string>& paramBindOrder,
      jsonObject_t& parameterBinds);

    inline bool hasBindingUploaded()
    {
      return m_hasBindingUploaded;
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
    * Check whether the session's temporary stage has been created, and create it
    * if not.
    *
    * @throws Exception if creating the stage fails
    */
    void createStageIfNeeded();

    /**
    * Build PUT statement string. Handle filesystem differences and escaping backslashes.
    * @param srcFilePath The faked source file path to upload.
    */
    std::string getPutStmt(const std::string& srcFilePath);

    /**
    * Convert time data format from nanoseconds to HH:MM:SS.F9
    * @param timeInNano The time data string in nanoseconds.
    */
    std::string convertTimeFormat(const std::string& timeInNano);

    /**
    * Convert date data format from days to YYYY-MM-DD
    * @param milliseconds since Epoch
    */
    std::string convertDateFormat(const std::string& millisecondSinceEpoch);

    /**
    * Convert timestamp data format from nanoseconds to YYYY_MM_DD HH:MM:SS.F9
    * @param timestampInNano The timestamp data string in nanoseconds.
    * @param type Either SQL_SF_TIMESTAMP_LTZ or NTZ depends on CLIENT_TIMESTAMP_TYPE_MAPPING
    */
    std::string convertTimestampFormat(const std::string& timestampInNano,
                                       simba_int16 type);

    /**
    * Revert time data format from HH:MM:SS.F9 to nanoseconds
    * @param formatedTime The time data string in HH:MM:SS.F9.
    */
    std::string revertTimeFormat(const std::string& formatedTime);

    /**
    * Convert date data format from YYYY-MM-DD to milliseconds since Epoch
    * @param formatedDate the date string in YYYY-MM-DD
    */
    std::string revertDateFormat(const std::string& formatedDate);

    /**
    * Convert timestamp data format from YYYY_MM_DD HH:MM:SS.F9 to nanoseconds
    * @param Formatedtimestamp The timestamp data string in YYYY_MM_DD HH:MM:SS.F9.
    * @param type Either SQL_SF_TIMESTAMP_LTZ or NTZ depends on CLIENT_TIMESTAMP_TYPE_MAPPING
    */
    std::string revertTimestampFormat(const std::string& Formatedtimestamp,
                                      simba_int16 type);

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

    Connection &m_connection;

    std::stringstream m_csvStream;

    std::stringstream m_compressStream;

    simba_wstring m_stagePath;

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

    bool m_injectError;
  };

} // namespace sf

#endif  // BINDUPLOADER_HPP
