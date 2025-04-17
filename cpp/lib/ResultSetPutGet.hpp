#ifndef SNOWFLAKECLIENT_RESULTSETPUTGET_HPP
#define SNOWFLAKECLIENT_RESULTSETPUTGET_HPP

#include "ResultSet.hpp"
#include "snowflake/ITransferResult.hpp"

namespace Snowflake
{
  namespace Client
  {

    /**
     * Represents a result set retrieved from PUT/GET execution.
     */
    class ResultSetPutGet : public Snowflake::Client::ResultSet
    {
    public:
      /**
       * Parameterized constructor.
       *
       * @param result                    A pointer to the transfer result.
       */
      ResultSetPutGet(ITransferResult *result);

      /**
       * Destructor.
       */
      ~ResultSetPutGet();

      /**
       * Setup column description of the transfer result.
       *
       * param desc The output parameter to return the pointer to the buffer allocated
       *            for the column description.
       *            Needs to be freed by the caller using SF_FREE.
       * param def_strlen The default size of string data.
       *
       * @return The number of columns.
       */
      size_t setup_column_desc(SF_COLUMN_DESC **desc, int64 def_strlen);

      /**
       * Advances the internal iterator to the next row. If there are no more rows to consume,
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL next();

      /**
       * Writes the value of the given cell as a boolean to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsBool(size_t idx, sf_bool *out_data);

      /**
       * Writes the value of the given cell as an int8 to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsInt8(size_t idx, int8 *out_data);

      /**
       * Writes the value of the given cell as an int32 to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsInt32(size_t idx, int32 *out_data);

      /**
       * Writes the value of the given cell as an int64 to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsInt64(size_t idx, int64 *out_data);

      /**
       * Writes the value of the given cell as a uint8 to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsUint8(size_t idx, uint8 *out_data);

      /**
       * Writes the value of the given cell as a uint32 to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsUint32(size_t idx, uint32 *out_data);

      /**
       * Writes the value of the given cell as a uint64 to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsUint64(size_t idx, uint64 *out_data);

      /**
       * Writes the value of the given cell as a float32 to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsFloat32(size_t idx, float32 *out_data);

      /**
       * Writes the value of the given cell as a float64 to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsFloat64(size_t idx, float64 *out_data);

      /**
       * Writes the value of the given cell as a constant C-string to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsConstString(size_t idx, const char **out_data);

      /**
       * Writes the value of the given cell as a timestamp to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellAsTimestamp(size_t idx, SF_TIMESTAMP *out_data);

      /**
       * Writes the length of the given cell to the provided buffer.
       *
       * @param idx                  The index of the column to retrieve.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL getCellStrlen(size_t idx, size_t *out_data);

      /**
       * Gets the total number of rows in the current chunk being processed.
       *
       * @return the number of rows in the current chunk.
       */
      size_t getRowCountInChunk();

      /**
       * Indicates whether the given cell is null.
       *
       * @param idx                  The index of the column to check is null.
       * @param out_data             The buffer to write to.
       *
       * @return 0 if successful, otherwise an error is returned.
       */
      SF_STATUS STDCALL isCellNull(size_t idx, sf_bool *out_data);

    private:
      // Hidden default constructor.
      ResultSetPutGet();

      // the result values from transfer result
      std::vector<std::vector<std::string>> m_values;

      // the command type of UPLOAD(PUT) or DOWNLOAD(GET)
      CommandType m_cmdType;
    };

  } // namespace Client
} // namespace Snowflake

#endif // SNOWFLAKECLIENT_RESULTSETPUTGET_HPP
