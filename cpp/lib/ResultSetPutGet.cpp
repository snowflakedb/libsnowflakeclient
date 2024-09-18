/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

#include "ResultSetPutGet.hpp"
#include "../logger/SFLogger.hpp"
#include "memory.h"
#include "client_int.h"

// helper functions
namespace
{
  void setup_string_column_desc(const std::string& name, SF_COLUMN_DESC& col_desc, size_t idx)
  {
    col_desc.name = (char*)SF_CALLOC(1, name.length() + 1);
    sf_strncpy(col_desc.name, name.length() + 1, name.c_str(), name.length() + 1);
    col_desc.byte_size = SF_DEFAULT_MAX_OBJECT_SIZE;
    col_desc.c_type = SF_C_TYPE_STRING;
    col_desc.internal_size = SF_DEFAULT_MAX_OBJECT_SIZE;
    col_desc.null_ok = SF_BOOLEAN_TRUE;
    col_desc.precision = 0;
    col_desc.scale = 0;
    col_desc.type = SF_DB_TYPE_TEXT;
    col_desc.idx = idx;
  }

  void setup_integer_column_desc(const std::string& name, SF_COLUMN_DESC& col_desc, size_t idx)
  {
    col_desc.name = (char*)SF_CALLOC(1, name.length() + 1);
    sf_strncpy(col_desc.name, name.length() + 1, name.c_str(), name.length() + 1);
    col_desc.byte_size = 8;
    col_desc.c_type = SF_C_TYPE_UINT64;
    col_desc.internal_size = 8;
    col_desc.null_ok = SF_BOOLEAN_TRUE;
    col_desc.precision = 38;
    col_desc.scale = 0;
    col_desc.type = SF_DB_TYPE_FIXED;
    col_desc.idx = idx;
  }

  struct putget_column
  {
    std::string name;
    bool isInteger;
  };

  std::vector<struct putget_column> PUTGET_COLUMNS[2] =
  {
    // UPLOAD
    {
      {"source", false},
      {"target", false},
      {"source_size", true},
      {"target_size", true},
      {"source_compression", false},
      {"target_compression", false},
      {"status", false},
      {"encryption", false},
      {"message", false},
    },
    // DOWNLOAD
    {
      {"file", false},
      {"size", true},
      {"status", false},
      {"encryption", false},
      {"message", false},
    },
  };
}

namespace Snowflake
{
namespace Client
{
ResultSetPutGet::ResultSetPutGet(ITransferResult *result) :
    ResultSet(SF_PUTGET_FORMAT),
    m_cmdType(result->getCommandType())
{
  m_values.reserve(result->getResultSize());
  while (result->next())
  {
    std::vector<std::string> row;
    row.reserve(result->getColumnSize());
    for (unsigned int i = 0; i < result->getColumnSize(); i++)
    {
      row.emplace_back();
      result->getColumnAsString(i, row.back());
    }
    m_values.push_back(row);
  }
}

ResultSetPutGet::~ResultSetPutGet()
{
  ; // Do nothing
}

// Public methods ==================================================================================

size_t ResultSetPutGet::setup_column_desc(SF_COLUMN_DESC** desc)
{
  if ((m_cmdType != CommandType::UPLOAD) && (m_cmdType != CommandType::DOWNLOAD))
  {
    // impossible
    return 0;
  }

  SF_COLUMN_DESC * col_desc = NULL;
  m_totalColumnCount = PUTGET_COLUMNS[m_cmdType].size();
  col_desc = (SF_COLUMN_DESC*)SF_CALLOC(m_totalColumnCount, sizeof(SF_COLUMN_DESC));
  for (size_t i = 0; i < m_totalColumnCount; i++)
  {
    if (PUTGET_COLUMNS[m_cmdType][i].isInteger)
    {
      setup_integer_column_desc(PUTGET_COLUMNS[m_cmdType][i].name, col_desc[i], i + 1);
    }
    else
    {
      setup_string_column_desc(PUTGET_COLUMNS[m_cmdType][i].name, col_desc[i], i + 1);
    }
  }

  *desc = col_desc;
  return m_totalColumnCount;
}

SF_STATUS STDCALL ResultSetPutGet::next()
{
  if (m_currRowIdx < m_values.size())
  {
    m_currRowIdx++;
    return SF_STATUS_SUCCESS;
  }

  return SF_STATUS_ERROR_OUT_OF_BOUNDS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsBool(size_t idx, sf_bool * out_data)
{
  setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
      "Value cannot be converted to boolean.");
  return SF_STATUS_ERROR_CONVERSION_FAILURE;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsInt8(size_t idx, int8 * out_data)
{
  VERIFY_COLUMN_INDEX(idx, m_totalColumnCount);
  size_t row_idx = m_currRowIdx > 0 ? m_currRowIdx - 1 : 0;
  *out_data = static_cast<int8>(m_values[row_idx][idx - 1][0]);
  return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsInt32(size_t idx, int32 * out_data)
{
  *out_data = 0;

  uint64 value = 0;
  SF_STATUS ret = getCellAsUint64(idx, &value);

  if (SF_STATUS_SUCCESS != ret)
  {
    return ret;
  }

  if (value > SF_INT32_MAX)
  {
    CXX_LOG_ERROR("Value out of range for int32.");
    setError(SF_STATUS_ERROR_OUT_OF_RANGE,
        "Value out of range for int32.");
    return SF_STATUS_ERROR_OUT_OF_RANGE;
  }

    *out_data = static_cast<int32>(value);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsInt64(size_t idx, int64 * out_data)
{
  *out_data = 0;

  uint64 value = 0;
  SF_STATUS ret = getCellAsUint64(idx, &value);

  if (SF_STATUS_SUCCESS != ret)
  {
    return ret;
  }

  if (value > SF_INT64_MAX)
  {
    CXX_LOG_ERROR("Value out of range for int64.");
    setError(SF_STATUS_ERROR_OUT_OF_RANGE,
        "Value out of range for int64.");
    return SF_STATUS_ERROR_OUT_OF_RANGE;
  }

    *out_data = static_cast<int64>(value);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsUint8(size_t idx, uint8 * out_data)
{
  return getCellAsInt8(idx, (int8*)out_data);
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsUint32(size_t idx, uint32 * out_data)
{
  *out_data = 0;

  uint64 value = 0;
  SF_STATUS ret = getCellAsUint64(idx, &value);

  if (SF_STATUS_SUCCESS != ret)
  {
    return ret;
  }

  if (value > SF_UINT32_MAX)
  {
    CXX_LOG_ERROR("Value out of range for uint32.");
    setError(SF_STATUS_ERROR_OUT_OF_RANGE,
        "Value out of range for uint32.");
    return SF_STATUS_ERROR_OUT_OF_RANGE;
  }

    *out_data = static_cast<uint32>(value);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsUint64(size_t idx, uint64 * out_data)
{
  VERIFY_COLUMN_INDEX(idx, m_totalColumnCount);

  *out_data = 0;

  if (!PUTGET_COLUMNS[m_cmdType][idx - 1].isInteger)
  {
    setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
        "Invalid conversion from string column to integer.");
    return SF_STATUS_ERROR_CONVERSION_FAILURE;
  }

  uint64 value = 0;
  size_t row_idx = m_currRowIdx > 0 ? m_currRowIdx - 1 : 0;

  try
  {
    value = std::stoull(m_values[row_idx][idx - 1], NULL, 10);
  }
  catch (...)
  {
    CXX_LOG_ERROR("Cannot convert value to uint64.");
    setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
        "Cannot convert value to uint64.");
    return SF_STATUS_ERROR_CONVERSION_FAILURE;
  }

  *out_data = value;
  return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsFloat32(size_t idx, float32 * out_data)
{
  *out_data = 0;

  uint64 value = 0;
  SF_STATUS ret = getCellAsUint64(idx, &value);

  if (SF_STATUS_SUCCESS != ret)
  {
    return ret;
  }

  *out_data = (float32)value;
  return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsFloat64(size_t idx, float64 * out_data)
{
  *out_data = 0;

  uint64 value = 0;
  SF_STATUS ret = getCellAsUint64(idx, &value);

  if (SF_STATUS_SUCCESS != ret)
  {
    return ret;
  }

  *out_data = (float64)value;
  return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsConstString(size_t idx, const char ** out_data)
{
  VERIFY_COLUMN_INDEX(idx, m_totalColumnCount);

  size_t row_idx = m_currRowIdx > 0 ? m_currRowIdx - 1 : 0;

  *out_data = m_values[row_idx][idx - 1].c_str();
  return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSetPutGet::getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data)
{
  setError(SF_STATUS_ERROR_CONVERSION_FAILURE,
    "Value cannot be converted to timestamp.");
  return SF_STATUS_ERROR_CONVERSION_FAILURE;
}

SF_STATUS STDCALL ResultSetPutGet::getCellStrlen(size_t idx, size_t * out_data)
{
  VERIFY_COLUMN_INDEX(idx, m_totalColumnCount);

  size_t row_idx = m_currRowIdx > 0 ? m_currRowIdx - 1 : 0;

  *out_data = m_values[row_idx][idx - 1].length();
  return SF_STATUS_SUCCESS;
}

size_t ResultSetPutGet::getRowCountInChunk()
{
    return m_values.size();
}

SF_STATUS STDCALL ResultSetPutGet::isCellNull(size_t idx, sf_bool * out_data)
{
  VERIFY_COLUMN_INDEX(idx, m_totalColumnCount);
  *out_data = SF_BOOLEAN_FALSE;

    return SF_STATUS_SUCCESS;
}

} // namespace Client
} // namespace Snowflake
