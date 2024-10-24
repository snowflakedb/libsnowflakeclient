/*
 * File:   BindUploader.cpp
 * Author: harryx
 *
 * Copyright (c) 2020 Snowflake Computing
 *
 * Created on March 5, 2020, 3:14 PM
 */

#include <sstream>
#include <iomanip>

#include "BindUploader.hpp"
#include "picojson.h"
#include "Logger.hpp"
#include "NumberConverter.h"
#include "Mutex.hpp"
#include "TDWTime.h"
#include "TDWDate.h"
#include "TDWTimestamp.h"
#include "Platform/DataConversion.hpp"
#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384
#define WINDOW_BIT 15
#define GZIP_ENCODING 16

namespace
{
  static const simba_wstring STAGE_NAME(L"SYSTEM$BIND");

  static const simba_wstring CREATE_STAGE_STMT(
    L"CREATE TEMPORARY STAGE "
    + STAGE_NAME
    + L" file_format=("
    + L" type=csv"
    + L" field_optionally_enclosed_by='\"'"
    + L")");

  static const simba_wstring PUT_STMT(
    L"PUT"
    L" file://%s"               // argument 1: fake file name
    L" '%s'"                    // argument 2: stage path
    L" overwrite=true"          // skip file existence check
    L" auto_compress=false"     // we compress already
    L" source_compression=gzip" // (with gzip)
  );

  static const unsigned int PUT_RETRY_COUNT = 3;
}

namespace sf
{
  using namespace picojson;
  using namespace Simba::Support;

  BindUploader::BindUploader(Connection &connection, const simba_wstring& stageDir,
                             unsigned int numParams, unsigned int numParamSets,
                             int compressLevel, bool injectError) :
    m_connection(connection),
    m_stagePath(L"@" + STAGE_NAME + L"/" + stageDir + L"/"),
    m_fileNo(0),
    m_retryCount(PUT_RETRY_COUNT),
    m_maxFileSize(connection.getStageBindMaxFileSize()),
    m_numParams(numParams),
    m_numParamSets(numParamSets),
    m_curParamIndex(0),
    m_curParamSetIndex(0),
    m_dataSize(0),
    m_startTime(std::chrono::steady_clock::now()),
    m_serializeStartTime(std::chrono::steady_clock::now()),
    m_compressTime(0),
    m_serializeTime(0),
    m_putTime(0),
    m_hasBindingUploaded(false),
    m_compressLevel(compressLevel),
    m_injectError(injectError)
  {
    SF_TRACE_LOG("sf", "BindUploader", "BindUploader",
      "Constructing BindUploader%s", "");
  }

  void BindUploader::putBinds()
  {
    // count serialize time since this function is called when serialization for
    // one chunk is done
    m_serializeTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_serializeStartTime).count();
    m_serializeStartTime = std::chrono::steady_clock::now();

    createStageIfNeeded();
    auto compressStartTime = std::chrono::steady_clock::now();
    size_t compressedSize = compressWithGzip();
    m_compressTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - compressStartTime).count();

    auto putStartTime = std::chrono::steady_clock::now();
    std::string filename = NumberConverter::ConvertUInt32ToString(m_fileNo++);
    while (m_retryCount > 0)
    {
      std::string putStmt = getPutStmt(filename);
      try
      {
        sf::Statement statement(m_connection);
        statement.setUploadStream(m_compressStream, compressedSize);
        statement.executeTransfer(putStmt);
        m_hasBindingUploaded = true;
        if (m_injectError && (m_fileNo == 1))
        {
          // throw error on second chunk uploading to test the logic of fallback
          // to regular binding
          SF_THROWGEN1_NO_INCIDENT(L"SFFileTransferError", "Error injection.");
        }
        break;
      }
      catch (...)
      {
        SF_TRACE_LOG("sf", "BindUploader", "putBinds",
          "Failed to upload array binds, retry%s", "");
        m_retryCount--;
        if (0 == m_retryCount)
        {
          SF_TRACE_LOG("sf", "BindUploader", "putBinds",
            "Failed to upload array binds with all retry%s", "");
          throw;
        }
      }
    }
    m_putTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - putStartTime).count();

    m_csvStream = std::stringstream();
    m_dataSize = 0;
    if (m_curParamSetIndex >= m_numParamSets)
    {
      auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_startTime).count();
      SF_INFO_LOG("sf", "BindUploader", "addStringValue",
        "total time: %ld, serialize time: %d, compress time: %ld, put time %ld", totalTime, m_serializeTime, m_compressTime, m_putTime);
    }
  }

  size_t BindUploader::compressWithGzip()
  {
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    m_compressStream = std::stringstream();
    m_csvStream.seekg(0);

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit2(&strm, m_compressLevel, Z_DEFLATED,
      WINDOW_BIT | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK)
    {
      SF_TRACE_LOG("sf", "BindUploader", "compressWithGzip",
        "Compression initial failed with error code %d", ret);
      throw;
    }

    /* compress until end of source data */
    do
    {
      m_csvStream.read((char*)in, CHUNK);
      strm.next_in = in;
      strm.avail_in = m_csvStream.gcount();
      flush = m_csvStream.eof() ? Z_FINISH : Z_NO_FLUSH;

      /* run deflate() on input until output buffer not full, finish
      compression if all of source has been read in */
      do
      {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = deflate(&strm, flush);    /* no bad return value */
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
        have = CHUNK - strm.avail_out;
        m_compressStream.write((char*)out, have);
      } while (strm.avail_out == 0);
      assert(strm.avail_in == 0);     /* all input will be used */

                                      /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    size_t destSize = strm.total_out;

    /* clean up and return */
    (void)deflateEnd(&strm);
    return destSize;
  }

  void BindUploader::createStageIfNeeded()
  {
    // Check the flag without locking to get better performance.
    if (m_connection.isArrayBindStageCreated())
    {
      return;
    }

    MutexGuard guard(m_connection.getArrayBindingMutex());

    // another thread may have created the session by the time we enter this block
    // so check the flag again.
    if (m_connection.isArrayBindStageCreated())
    {
      return;
    }

    try
    {
      sf::Statement statement(m_connection);
      statement.executeQuery(CREATE_STAGE_STMT.GetAsUTF8(), false, true);
      m_connection.setArrayBindStageCreated();
    }
    catch (...)
    {
      SF_TRACE_LOG("sf", "BindUploader", "createStageIfNeeded",
        "Failed to create temporary stage for array binds.%s", "");
      throw;
    }
  }

  std::string BindUploader::getPutStmt(const std::string& srcFilePath)
  {
    char strBuf[MAX_PATH * 2]; // *2 to make sure there is enough space
    simba_sprintf(strBuf, sizeof(strBuf), PUT_STMT.GetAsUTF8().c_str(),
                  srcFilePath.c_str(), getStagePath().c_str());

    return std::string(strBuf);
  }

  std::string BindUploader::convertTimeFormat(const std::string& timeInNano)
  {
    simba_uint32 seconds;
    simba_uint32 fraction;
    int len = timeInNano.length();
    if (len < 10)
    {
      seconds = 0;
      fraction = NumberConverter::ConvertStringToUInt32(timeInNano);
    }
    else
    {
      seconds = NumberConverter::ConvertStringToUInt32(timeInNano.substr(0, len - 9));
      fraction = NumberConverter::ConvertStringToUInt32(timeInNano.substr(len - 9));
    }

    simba_uint16 hour, min, sec;
    hour = seconds / 3600;
    seconds = seconds % 3600;
    min = seconds / 60;
    sec = seconds % 60;
    TDWTime time(hour, min, sec, fraction);

    return time.ToString(9);
  }

  std::string BindUploader::revertTimeFormat(const std::string& formatedTime)
  {
    TDWTime time(formatedTime);
    std::string seconds = std::to_string(time.Hour * 3600 + time.Minute * 60 + time.Second);
    std::string fraction = std::to_string(time.Fraction);
    if (fraction.length() < 9)
    {
      fraction = std::string(9 - fraction.length(), '0') + fraction;
    }
    return seconds + fraction;
  }

  std::string BindUploader::convertDateFormat(const std::string& millisecondSinceEpoch)
  {
    simba_int64 SecondsSinceEpoch =
      NumberConverter::ConvertStringToInt64(millisecondSinceEpoch) / 1000;
    TDWDate date = sf::DataConversions::parseSnowflakeDate(SecondsSinceEpoch);
    return date.ToString();
  }

  std::string BindUploader::revertDateFormat(const std::string& formatedDate)
  {
    TDWDate date(formatedDate);
    struct tm datetm;
    datetm.tm_year = date.Year -1900;
    datetm.tm_mon = date.Month - 1;
    datetm.tm_mday = date.Day;
    datetm.tm_hour = 0;
    datetm.tm_min = 0;
    datetm.tm_sec = 0;

    simba_int64 secondsSinceEpoch = (simba_int64)sf::DataConversions::sfchrono_timegm(&datetm);
    return std::to_string(secondsSinceEpoch * 1000);
  }

  std::string BindUploader::convertTimestampFormat(const std::string& timestampInNano,
                                                   simba_int16 type)
  {
    TDWExactNumericType totalFracSeconds(timestampInNano.c_str(),
                                         timestampInNano.length(),
                                         true);
    totalFracSeconds.MultiplyByTenToThePowerOf(-9);

    sb8 seconds = totalFracSeconds.GetInt64();
    bool isTruncated;
    bool isOutofRange;
    simba_uint32 fraction = totalFracSeconds.GetFraction(isTruncated, isOutofRange, 9);
    if (!totalFracSeconds.IsPositive() && (fraction != 0))
    {
      seconds--;
      fraction = 1000000000 - fraction;
    }

    TDWTimestamp timestamp;
    LogicalType_t ltype;
    if (type == SQL_SF_TIMESTAMP_NTZ)
    {
      ltype = LTY_TIMESTAMP_NTZ;
    }
    else
    {
      ltype = LTY_TIMESTAMP_LTZ;
    }

    timestamp = sf::DataConversions::parseSnowflakeTimestamp(
                    seconds,
                    fraction,
                    ltype,
                    9,
                    true,
                    true);

    // Get the local time offset
    tm tmV;
    time_t timeV = (time_t)seconds;
    int offset = 0;
    sf::DataConversions::sfchrono_localtime(&timeV, &tmV);
#if defined(WIN32) || defined(_WIN64)
    sb8 localEpoch = (sf::sb8)(sf::DataConversions::sfchrono_timegm(&tmV));
    offset = (int)(localEpoch - (sf::sb8)seconds);
#else
    offset = tmV.tm_gmtoff;
#endif
    int tzh = offset / 3600;
    int tzm = (offset - (tzh * 3600)) / 60;
    std::ostringstream stz;
    stz << ((offset < 0) ? "-" : "+")
      << std::setfill('0') << std::setw(2) << abs(tzh)
      << ":" << std::setfill('0') << std::setw(2) << abs(tzm);

    return timestamp.ToString() + " " + stz.str();
  }

  std::string BindUploader::revertTimestampFormat(const std::string& formatedtTimestamp, simba_int16 type)
  {
    // separate timestamp and timezone information
    // this is reverting the output from convertTimestampFormat so we should
    // always have timezone part lead with a space
    size_t timezonePos = formatedtTimestamp.rfind(' ');
    if (timezonePos == std::string::npos)
    {
      // not possible but just in case
      return "";
    }

    simba_wstring timestampStr = formatedtTimestamp.substr(0, timezonePos);
    timestampStr.Trim();
    TDWTimestamp timestamp(timestampStr);
    struct tm gmttm;
    gmttm.tm_year = timestamp.Year - 1900;
    gmttm.tm_mon = timestamp.Month - 1;
    gmttm.tm_mday = timestamp.Day;
    gmttm.tm_hour = timestamp.Hour;
    gmttm.tm_min = timestamp.Minute;
    gmttm.tm_sec = timestamp.Second;
    simba_int64 secondsSinceEpoch = (simba_int64)sf::DataConversions::sfchrono_timegm(&gmttm);

    // For local timezone add timezone information to get gmt time.
    if (type != SQL_SF_TIMESTAMP_NTZ)
    {
      simba_wstring timezoneStr = formatedtTimestamp.substr(timezonePos);
      timezoneStr.Trim();
      bool isTimezoneSigned = false;
      if ((timezoneStr.GetLength() > 0) &&
        ((timezoneStr.CharAt(0) == '+') || (timezoneStr.CharAt(0) == '-')))
      {
        if (timezoneStr.CharAt(0) == '-')
        {
          isTimezoneSigned = true;
        }
        timezoneStr = timezoneStr.Substr(1) + ":00";
      }
      TDWTime timezone(timezoneStr);
      int timezoneSeconds = timezone.Hour * 3600 + timezone.Minute * 60;
      if (isTimezoneSigned)
      {
        timezoneSeconds *= -1;
      }
      secondsSinceEpoch -= timezoneSeconds;
    }

    // If the seconds is negative, convert fraction to negative value as well.
    simba_uint32 fraction = timestamp.Fraction;
    if ((secondsSinceEpoch < 0) && (fraction > 0))
    {
      fraction = 1000000000 - fraction;
      secondsSinceEpoch++;
    }

    // return string in nano seconds combining second and fraction parts
    std::string fractionStr = std::to_string(fraction);
    if (fractionStr.length() < 9)
    {
      fractionStr = std::string(9 - fractionStr.length(), '0') + fractionStr;
    }

    return std::to_string(secondsSinceEpoch) + fractionStr;
  }

  void BindUploader::addStringValue(const std::string& val, simba_int16 type)
  {
    if (m_curParamIndex != 0)
    {
      m_csvStream << ",";
      m_dataSize++;
    }
    else if (m_dataSize == 0)
    {
      m_serializeStartTime = std::chrono::steady_clock::now();
    }

    if (val.empty())
    {
      m_csvStream << "\"\""; // an empty string => an empty string with quotes
      m_dataSize += sizeof("\"\"");
    }
    else
    {
      switch (type)
      {
        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
          std::string timeStr = convertTimeFormat(val);
          m_csvStream << timeStr;
          m_dataSize += timeStr.length();
          break;
        }

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
          std::string dateStr = convertDateFormat(val);
          m_csvStream << dateStr;
          m_dataSize += dateStr.length();
          break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        case SQL_SF_TIMESTAMP_LTZ:
        case SQL_SF_TIMESTAMP_NTZ:
        {
          std::string timestampStr = convertTimestampFormat(val, type);
          m_csvStream << timestampStr;
          m_dataSize += timestampStr.length();
          break;
        }

        default:
        {
          if (std::string::npos == val.find_first_of("\"\n,\\"))
          {
            m_csvStream << val;
            m_dataSize += val.length();
          }
          else
          {
            simba_wstring escapeSimbaStr(val);
            escapeSimbaStr.Replace("\"", "\"\"");
            escapeSimbaStr = "\"" + escapeSimbaStr + "\"";
            std::string escapeStr = escapeSimbaStr.GetAsUTF8();

            m_csvStream << escapeStr;
            m_dataSize += escapeStr.length();
          }
          break;
        }
      }
    }

    // The last column in the current row, add new line
    // Also upload the data as needed.
    if (++m_curParamIndex >= m_numParams)
    {
      m_csvStream << "\n";
      m_dataSize++;
      m_curParamIndex = 0;
      m_curParamSetIndex++;

      // Upload data when exceed file size limit or all rows are added
      if ((m_dataSize >= m_maxFileSize) ||
        (m_curParamSetIndex >= m_numParamSets))
      {
        putBinds();
      }
    }
  }

  void BindUploader::addNullValue()
  {
    if (m_curParamIndex != 0)
    {
      m_csvStream << ",";
      m_dataSize++;
    }

    // The last column in the current row, add new line
    // Also upload the data as needed.
    if (++m_curParamIndex >= m_numParams)
    {
      m_csvStream << "\n";
      m_dataSize++;
      m_curParamIndex = 0;
      m_curParamSetIndex++;

      // Upload data when exceed file size limit or all rows are added
      if ((m_dataSize >= m_maxFileSize) ||
        (m_curParamSetIndex >= m_numParamSets))
      {
        putBinds();
      }
    }
  }

  bool BindUploader::csvGetNextField(std::string& fieldValue,
                                     bool& isNull, bool& isEndOfRow)
  {
    char c;

    // the flag indecate if currently in a quoted value
    bool inQuote = false;
    // the flag indecate if the value has been quoted, quoted empty string is
    // empty value (like ,"",) while unquoted empty string is null (like ,,)
    bool quoted = false;
    // the flag indecate a value is found to end the loop
    bool done = false;
    // the flag indicate the next char already fetched by checking double quote escape ("")
    bool nextCharFetched = false;

    fieldValue.clear();

    if (!m_csvStream.get(c))
    {
      return false;
    }

    while (!done)
    {
      switch (c)
      {
        case ',':
        {
          if (!inQuote)
          {
            done = true;
          }
          else
          {
            fieldValue.push_back(c);
          }
          break;
        }

        case '\n':
        {
          if (!inQuote)
          {
            done = true;
            isEndOfRow = true;
          }
          else
          {
            fieldValue.push_back(c);
          }
          break;
        }

        case '\"':
        {
          if (!inQuote)
          {
            quoted = true;
            inQuote = true;
          }
          else
          {
            if (!m_csvStream.get(c))
            {
              isEndOfRow = true;
              done = true;
            }
            else
            {
              if (c == '\"')
              {
                // escape double qoute in quoted string
                fieldValue.push_back(c);
              }
              else
              {
                inQuote = false;
                nextCharFetched = true;
              }
            }
          }

          break;
        }

        default:
        {
          fieldValue.push_back(c);
        }
      }

      if ((!done) && (!nextCharFetched))
      {
        if (!m_csvStream.get(c))
        {
          isEndOfRow = true;
          break;
        }
      }
      else
      {
        nextCharFetched = false;
      }
    }

    isNull = (fieldValue.empty() && !quoted);
    return true;
  }

  void BindUploader::convertBindingFromCsvToJson(
    std::vector<std::string>& paramBindOrder, jsonObject_t& parameterBinds)
  {
    bool endOfData = false;
    m_csvStream.seekg(0);

    while (!endOfData)
    {
      bool endOfRow = false;
      for (size_t i = 0; i < paramBindOrder.size(); i++)
      {
        std::string fieldValue;
        bool isNull = false;
        std::string bindName = paramBindOrder[i];
        jsonObject_t& bind = parameterBinds[bindName].get<object&>();
        jsonArray_t& valueList = bind["value"].get<array&>();
        const std::string& colType = bind["type"].get<std::string&>();

        // Should not happen as we are parsing the result generated by
        // addStringValue()/addNullValue(), fill missing fields with null
        // just in case
        if (endOfData || endOfRow)
        {
          valueList.push_back(value());
          continue;
        }

        if (!csvGetNextField(fieldValue, isNull, endOfRow))
        {
          endOfData = true;
          if (i == 0)
          {
            // Normal case of reaching end of data
            break;
          }
          // Should not happen but just fill the missing data with null
          valueList.push_back(value());
          continue;
        }

        if (isNull)
        {
          valueList.push_back(value());
          continue;
        }

        if (strcasecmp(colType.c_str(), "TIME") == 0)
        {
          fieldValue = revertTimeFormat(fieldValue);
        }
        else if (strcasecmp(colType.c_str(), "DATE") == 0)
        {
          fieldValue = revertDateFormat(fieldValue);
        }
        else if (strcasecmp(colType.c_str(), "TIMESTAMP") == 0)
        {
          fieldValue = revertTimestampFormat(fieldValue, SQL_SF_TIMESTAMP_LTZ);
        }
        else if (strcasecmp(colType.c_str(), "TIMESTAMP_NTZ") == 0)
        {
          fieldValue = revertTimestampFormat(fieldValue, SQL_SF_TIMESTAMP_NTZ);
        }

        valueList.push_back(value(fieldValue));
      }
    }
  }
}  // namespace sf
