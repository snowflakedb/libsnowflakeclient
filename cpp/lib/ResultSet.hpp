/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_RESULTSET_HPP
#define SNOWFLAKECLIENT_RESULTSET_HPP

#include <cstddef>
#include <memory>
#include <string>

#include "cJSON.h"
#include "snowflake/basic_types.h"
#include "snowflake/client.h"
#include "result_set.h"

#define VERIFY_COLUMN_INDEX(index, total)                             \
  if (index < 1 || index > total)                                     \
  {                                                                   \
    setError(SF_STATUS_ERROR_OUT_OF_BOUNDS,                           \
      "Column index must be between 1 and snowflake_num_fields()");   \
    return SF_STATUS_ERROR_OUT_OF_BOUNDS;                             \
  }

namespace Snowflake
{
namespace Client
{

/**
 * The implementation of a base result set.
 *
 * We use a partition the results into chunks, where a chunk is simply a maximum
 * unit of memory that may be retrieved and operated on at any given moment.
 */
class ResultSet
{
public:

    /**
     * Default constructor.
     */
    ResultSet(QueryResultFormat format);

    /**
     * Parameterized constructor.
     *
     * @param metadata                  The metadata of the result set.
     * @param tzString                  The time zone.
     */
    ResultSet(SF_COLUMN_DESC * metadata, const std::string& tzString, QueryResultFormat format);

    static ResultSet* CreateResultFromJson(cJSON* json_rowset,
                                           SF_COLUMN_DESC* metadata,
                                           QueryResultFormat query_result_format,
                                           const std::string& tz_string);

    static ResultSet* CreateResultFromChunk(void* initial_chunk,
                                            SF_COLUMN_DESC* metadata,
                                            QueryResultFormat query_result_format,
                                            const std::string& tz_string);

    /**
     * Destructor.
     */
    virtual ~ResultSet(){}

    // API methods =================================================================================

    /**
     * Frees the previously held chunk in m_chunk and sets the current chunk to the given chunk.
     * Each result type should override this function to handle chunks,
     * while some result might not apply (won't be called), return error by default.
     *
     * @param chunkPtr                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL appendChunk(void* chunkPtr)
    {
      return SF_STATUS_ERROR_GENERAL;
    }

    /**
     * Advances to the next row.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL next() = 0;

    /**
     * Writes the value of the given cell as a boolean to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsBool(size_t idx, sf_bool * out_data) = 0;

    /**
     * Writes the value of the given cell as an int8 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsInt8(size_t idx, int8 * out_data) = 0;

    /**
     * Writes the value of the given cell as an int32 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsInt32(size_t idx, int32 * out_data) = 0;

    /**
     * Writes the value of the given cell as an int64 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsInt64(size_t idx, int64 * out_data) = 0;

    /**
     * Writes the value of the given cell as a uint8 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsUint8(size_t idx, uint8 * out_data) = 0;

    /**
     * Writes the value of the given cell as a uint32 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsUint32(size_t idx, uint32 * out_data) = 0;

    /**
     * Writes the value of the given cell as a uint64 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsUint64(size_t idx, uint64 * out_data) = 0;

    /**
     * Writes the value of the given cell as a float32 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsFloat32(size_t idx, float32 * out_data) = 0;

    /**
     * Writes the value of the given cell as a float64 to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsFloat64(size_t idx, float64 * out_data) = 0;

    /**
     * Writes the value of the given cell as a constant C-string to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsConstString(size_t idx, const char ** out_data) = 0;

    /**
     * Writes the value of the given cell as a timestamp to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data) = 0;

    /**
     * Writes the length of the given cell to the provided buffer.
     *
     * @param idx                  The index of the column or row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL getCellStrlen(size_t idx, size_t * out_data) = 0;

    /**
     * Indicates whether the given cell is null.
     *
     * @param idx                  The index of the column or row to check is null.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    virtual SF_STATUS STDCALL isCellNull(size_t idx, sf_bool * out_data) = 0;

    /**
     * Gets the total number of rows in the current chunk being processed.
     *
     * @return the number of rows in the current chunk.
     */
    virtual size_t getRowCountInChunk() = 0;

    // Other member getters ========================================================================

    SF_STATUS getError()
    {
        return m_error;
    }

    const char * getErrorMessage()
    {
        return m_errMsg.c_str();
    }

    void setError(SF_STATUS error, const char* errMsg)
    {
        m_error = error;
        if (errMsg)
        {
            m_errMsg = errMsg;
        }
    }

    QueryResultFormat getResultFormat()
    {
        return m_queryResultFormat;
    }

protected:

    // Protected members ===========================================================================

    // Output formats ==============================================================================

    /**
     * The output format of a Binary field.
     */
    std::string m_binaryOutputFormat;

    /**
     * The output format of a Date field.
     */
    std::string m_dateOutputFormat;

    /**
     * The output format of a Time field.
     */
    std::string m_timeOutputFormat;

    /**
     * The output format of a Timestamp field.
     */
    std::string m_timestampOutputFormat;

    /**
     * The output format of a Timestamp LTZ (local time zone) field.
     */
     std::string m_timestampLtzOutputFormat;

    /**
     * The output format of a Timestamp NTZ (no time zone) field.
     */
     std::string m_timestampNtzOutputFormat;

    /**
     * The output format of a Timestamp TZ (time zone) field.
     */
     std::string m_timestampTzOutputFormat;

    // Counts and indices ==========================================================================

    /**
     * The index of the current chunk.
     */
    size_t m_currChunkIdx;

    /**
     * The index of the current row in the current chunk.
     */
    size_t m_currChunkRowIdx;

    /**
     * The index of the current column.
     */
    size_t m_currColumnIdx;

    /**
     * The index of the current row.
     */
    size_t m_currRowIdx;

    /**
     * The total number of chunks.
     */
    size_t m_totalChunkCount;

    /**
     * The total number of columns.
     */
    size_t m_totalColumnCount;

    /**
     * The total number of rows.
     */
    size_t m_totalRowCount;

    // Miscellaneous ===============================================================================

    /**
     * The metadata object.
     */
    SF_COLUMN_DESC * m_metadata;

    /**
     * The format of the result set.
     */
    QueryResultFormat m_queryResultFormat;

    /**
     * The tz string to use when dealing with time or timestamp values.
     */
    std::string m_tzString;

    /**
     * The tz offset to use when dealing with time or timestamp values.
     */
    int32 m_tzOffset;

    /**
     * Indicates whether the first chunk has been processed or not.
     */
    bool m_isFirstChunk;

    SF_STATUS m_error;

    std::string m_errMsg;
};

}
}

#endif // SNOWFLAKECLIENT_RESULTSET_HPP
