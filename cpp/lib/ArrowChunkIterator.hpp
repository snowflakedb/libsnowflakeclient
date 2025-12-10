#ifndef SNOWFLAKECLIENT_ARROWCHUNKITERATOR_HPP
#define SNOWFLAKECLIENT_ARROWCHUNKITERATOR_HPP

#include "arrowheaders.hpp"

#ifndef SF_WIN32

#include <map>
#include <memory>
#include <vector>
#include <string>

#include "snowflake/basic_types.h"
#include "snowflake/client.h"

namespace Snowflake
{
namespace Client
{
    class ResultSetArrow;
}
}

namespace Snowflake
{
namespace Client
{

/**
 * Represents a Timestamp value.
 *
 * Contains the following values:
 *
 * - seconds since Epoch,
 * - fractional seconds, expressed in nanoseconds,
 * - time zone offset.
 *
 * Timestamp string syntax:
 *   "{seconds_since_epoch}.{fractional_seconds} {time_offset}",
 * where the time offset field is optional*
 *
 * The time offset will never be negative, even for negative time offsets.
 * Instead, UTC+0 is stored as 24*60=1440. Thus, values lie in [0, 2880]*
 *
 * Examples:
 *   "2014-05-03 13:56:46.123  +09:00" => "1399093006.12300 1980"
 *   "1969-11-21 05:17:23.0123 -01:00" => "-3519756.98770 1380
 */
 struct ArrowTimestampArray
 {
    arrow::Int64Array * sse = nullptr;
    arrow::Int32Array * fs = nullptr;
    arrow::Int32Array * tz = nullptr;
 };

/**
 * Represents a column of the result set in Arrow format.
 */
struct ArrowColumn
{
    // The array data of the columns. Each instance of ArrowColumn should only have one populated.
    arrow::StructArray     * arrowStructArray;
    arrow::BinaryArray     * arrowBinary;
    arrow::BooleanArray    * arrowBoolean;
    arrow::Date32Array     * arrowDate32;
    arrow::Date64Array     * arrowDate64;
    arrow::Decimal128Array * arrowDecimal128;
    arrow::DoubleArray     * arrowDouble;
    arrow::Int8Array       * arrowInt8;
    arrow::Int16Array      * arrowInt16;
    arrow::Int32Array      * arrowInt32;
    arrow::Int64Array      * arrowInt64;
    arrow::StringArray     * arrowString;
    std::shared_ptr<ArrowTimestampArray> arrowTimestamp;
};

/**
 * Arrow chunk iterator implementation in C++.
 *
 * A utility class which abstracts the traversal of the vector of RecordBatch
 * objects which comprise an Arrow-format result set retrieved from the server.
 *
 * Mostly the same as the implementation in ODBC but with some changes.
 *
 * When the session parameter `C_API_QUERY_RESULT_FORMAT` is set to ARROW,
 * the Snowflake server will respond to queries with Arrow-formatted results.
 *
 * More specifically, the data retrieved is of the type:
 *     std::vector<std::shared_ptr<Arrow::RecordBatch>>.
 *
 * Further, this data is encoded in Base64. Decoding is NOT handled.
 *
 * Below is a sample server response in Arrow format:
 *
 * {
 *     "data": {
 *         "parameters": [...],
 *         "rowtype": [
 *              {
 *                  "name": "LEFT('LEFT IS RETURNED', 16)",
 *                  "database": "",
 *                  "schema": "",
 *                  "table": "",
 *                  "scale": null,
 *                  "precision": null,
 *                  "length": 16,
 *                  "type": "test",
 *                  "nullable": false,
 *                  "byteLength": 64,
 *                  "collation": null
 *              }
 *         ],
 *         "rowsetBase64": "/////1ABAAAQAAAAAAA...",
 *         "total": 5000,
 *         "returned": 5000,
 *         "queryId": "01999ca4-0391-126f-0000-000800016e32",
 *         "queryResultFormat": "arrow"
 *      },
 *      "code": null,
 *      "message": null,
 *      "success": true
 * }
 */
class ArrowChunkIterator
{
public:

    /**
     * Default constructor.
     *
     * Generally not used.
     */
    ArrowChunkIterator();

    /**
     * Parameterized constructor.
     *
     * @param chunk           The chunk
     * @param metadata        The column metadata retrieved from the Snowflake DB.
     * @param tzString        The time zone.
     */
    ArrowChunkIterator(arrow::BufferBuilder * chunk,
                       SF_COLUMN_DESC * metadata, std::string tzString,
                       ResultSetArrow * parent);

    /**
     * Destructor.
     */
    ~ArrowChunkIterator() = default;

    // Move to next row. return false when entire chunk is fetched.
    bool next();
    /**
    * Check a column of current row is null.
    *
    * @param col             The index of the column to check.
    *
    * @return true if the cell is null, otherwise false.
    */
    bool isCellNull(int32 col);

    /**
     * Gets the value of the given cell as an sf_bool.
     *
     * This API method supports conversion for the following data types:
     * - BOOLEAN
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsBool(size_t colIdx,sf_bool * out_data);

    /**
     * Gets the value of the given cell as an int8.
     *
     * This API method supports conversion for the following data types:
     * - BOOLEAN
     * - DATE32, DATE64
     * - DECIMAL128
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt8(size_t colIdx, int8 * out_data);

    /**
     * Gets the value of the given cell as an int32.
     *
     * This API method supports conversion for the following data types:
     * - BOOLEAN
     * - DATE32, DATE64
     * - DECIMAL128
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt32(size_t colIdx, int32 * out_data);

    /**
     * Gets the value of the given cell as an int64.
     *
     * This API method supports conversion for the following data types:
     * - BOOLEAN
     * - DATE32, DATE64
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     * @param rawData              The flag indicate that get the raw data directly without conversion.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt64(size_t colIdx, int64 * out_data, bool rawData = false);

    /**
     * Gets the value of the given cell as a uint8.
     *
     * This API method supports conversion for the following data types:
     * - BOOLEAN
     * - DATE32, DATE64
     * - DECIMAL128
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint8(size_t colIdx, uint8 * out_data);

    /**
     * Gets the value of the given cell as a uint32.
     *
     * This API method supports conversion for the following data types:
     * - BOOLEAN
     * - DATE32, DATE64
     * - DECIMAL128
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint32(size_t colIdx, uint32 * out_data);

    /**
     * Gets the value of the given cell as a uint64.
     *
     * This API method supports conversion for the following data types:
     * - BOOLEAN
     * - DATE32, DATE64
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint64(size_t colIdx, uint64 * out_data);

    /**
     * Gets the value of the given cell as a float32.
     *
     * This API method supports conversion for the following data types:
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsFloat32(size_t colIdx, float32 * out_data);

    /**
     * Gets the value of the given cell as a float64.
     *
     * This API method supports conversion for the following data types:
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsFloat64(size_t colIdx, float64 * out_data);

    /**
     * Gets the value of the given cell as a string.
     *
     * This API method supports conversion for the following data types:
     * - BINARY
     * - BOOLEAN
     * - DATE32, DATE64
     * - DECIMAL128
     * - INT8, INT16, INT32, INT64
     * - STRUCT
     *
     * @param colIdx               The index of the column to get.
     * @param outString            The output string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsString(
        size_t colIdx,
        std::string& outString);

    /**
     * Gets the value of the given cell as a timestamp.
     *
     * This API method supports conversion for the following data types:
     * - Timestamp
     *
     * @param colIdx               The index of the column to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsTimestamp(size_t colIdx, SF_TIMESTAMP * out_data);

    /**
     * Gets the row count of the current chunk.
     *
     * Used in conjunction with the library function:
     * - snowflake_affected_rows(), which retrieves the rowcount of the current chunk.
     * - snowflake_execute_ex(), which retrieves the rowcount of the current chunk.
     */
    size_t getRowCountInChunk();

    uint32 getColumnCount()
    {
        return m_columnCount;
    }

protected:
    /** schema of current record batch */
    std::shared_ptr<arrow::Schema> m_currentSchema;

    std::vector<std::shared_ptr<arrow::RecordBatch> > m_cRecordBatches;
    /**
     * Vector of ArrowColumns that represents the final, columnar result set.
     */
    std::vector<ArrowColumn> m_columns;

private:

    /** Column chunks */
    void initColumnChunks();

    // Private members =============================================================================

    /**
     * Total number of record batches in the result set.
     */
    uint32 m_batchCount;

    /**
     * Total number of columns.
     */
    uint32 m_columnCount;

    /**
     * Total number of rows inside current record batch.
     */
    uint32 m_rowCountInBatch;

    /**
     * Current index that the iterator points to.
     */
    uint32 m_currBatchIndex;

    /**
     * Row index inside current record batch. Zero-indexed.
     * Internal use only.
     */
    uint32 m_currRowIndexInBatch;

    /**
    * column data types
    */
    std::vector<uint8> m_arrowColumnDataTypes;

    /**
     * A pointer to the column metadata retrieved from the Snowflake DB.
     */
    SF_COLUMN_DESC * m_metadata;

    /**
     * The tz string to use when dealing with time or timestamp values.
     */
    std::string m_tzString;

    ResultSetArrow * m_parent;
};

} // namespace Client
} // namespace Snowflake

#endif  // SF_WIN32
#endif  // SNOWFLAKECLIENT_ARROWCHUNKITERATOR_HPP
