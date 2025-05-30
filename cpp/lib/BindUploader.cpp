#include <sstream>
#include <iomanip>
#include "zlib.h"

#include "snowflake/BindUploader.hpp"
#include "../logger/SFLogger.hpp"
#include "snowflake/basic_types.h"
#include "snowflake/SF_CRTFunctionSafe.h"
#include "../util/SnowflakeCommon.hpp"

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

using namespace Snowflake::Client::Util;

namespace
{
  static const std::string STAGE_NAME("SYSTEM$BIND");

  static const std::string CREATE_STAGE_STMT(
    "CREATE TEMPORARY STAGE "
    + STAGE_NAME
    + " file_format=("
    + " type=csv"
    + " field_optionally_enclosed_by='\"'"
    + ")");

  static const std::string PUT_STMT(
    "PUT"
    " file://%s"               // argument 1: fake file name
    " '%s'"                    // argument 2: stage path
    " overwrite=true"          // skip file existence check
    " auto_compress=false"     // we compress already
    " source_compression=gzip" // (with gzip)
  );
}

namespace Snowflake
{
namespace Client
{
BindUploader::BindUploader(const std::string& stageDir,
                           unsigned int numParams, unsigned int numParamSets,
                           unsigned int maxFileSize,
                           int compressLevel) :
  m_stagePath("@" + STAGE_NAME + "/" + stageDir + "/"),
  m_fileNo(0),
  m_maxFileSize(maxFileSize),
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
  m_compressLevel(compressLevel)
{
  CXX_LOG_TRACE("Constructing BindUploader: stageDir:%s, numParams: %d, numParamSets: %d, "
                "maxFileSize: %d, compressLevel: %d",
                stageDir.c_str(), numParams, numParamSets,
                maxFileSize, compressLevel);
}

bool BindUploader::putBinds()
{
  // count serialize time since this function is called when serialization for
  // one chunk is done
  m_serializeTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_serializeStartTime).count();
  m_serializeStartTime = std::chrono::steady_clock::now();

  if (!createStageIfNeeded())
  {
    return false;
  }

  auto compressStartTime = std::chrono::steady_clock::now();
  size_t compressedSize = compressWithGzip();
  m_compressTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - compressStartTime).count();

  auto putStartTime = std::chrono::steady_clock::now();
  std::string filename = std::to_string(m_fileNo++);
  std::string putStmt = getPutStmt(filename);
  // No retry needed here as PUT command execution already has retries.
  if (!executeUploading(putStmt, m_compressStream, compressedSize))
  {
    return false;
  }

  m_hasBindingUploaded = true;
  m_putTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - putStartTime).count();

  m_csvStream = std::stringstream();
  m_dataSize = 0;
  if (m_curParamSetIndex >= m_numParamSets)
  {
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_startTime).count();
    CXX_LOG_INFO("BindUploader::putBinds: total time: %ld, serialize time: %d, compress time: %ld, put time %ld",
                 totalTime, m_serializeTime, m_compressTime, m_putTime);
  }

  return true;
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
    CXX_LOG_TRACE("BindUploader: Compression initial failed with error code %d", ret);
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
      have = CHUNK - strm.avail_out;
      m_compressStream.write((char*)out, have);
    } while (strm.avail_out == 0);

                                    /* done when last data in file processed */
  } while (flush != Z_FINISH);

  size_t destSize = strm.total_out;

  /* clean up and return */
  (void)deflateEnd(&strm);
  return destSize;
}

std::string BindUploader::getPutStmt(const std::string& srcFilePath)
{
  char strBuf[MAX_PATH * 2]; // *2 to make sure there is enough space
  sf_sprintf(strBuf, sizeof(strBuf), PUT_STMT.c_str(),
             srcFilePath.c_str(), getStagePath().c_str());

  return std::string(strBuf);
}

std::string BindUploader::getCreateStageStmt()
{
  return CREATE_STAGE_STMT;
}

bool BindUploader::addStringValue(const std::string& val, SF_DB_TYPE type)
{
  SF_UNUSED(type);

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
    if (std::string::npos == val.find_first_of("\"\n,\\"))
    {
      m_csvStream << val;
      m_dataSize += val.length();
    }
    else
    {
      std::string escapeStr(val);
      replaceStrAll(escapeStr, "\"", "\"\"");
      escapeStr = "\"" + escapeStr + "\"";

      m_csvStream << escapeStr;
      m_dataSize += escapeStr.length();
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
      return putBinds();
    }
  }

  return true;
}

bool BindUploader::addNullValue()
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
      return putBinds();
    }
  }

  return true;
}

} // namespace Client
} // namespace Snowflake
