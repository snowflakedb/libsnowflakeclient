/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All Rights Reserved
 */
#ifndef SNOWFLAKECLIENT_ARROWCHUNKITERATOR_HPP
#define SNOWFLAKECLIENT_ARROWCHUNKITERATOR_HPP

#include <map>
#include <memory>
#include <vector>
#include <string>

#include "arrowheaders.hpp"

#include "snowflake/basic_types.h"
#include "snowflake/client.h"


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
    std::shared_ptr<arrow::Int64Array> sse;
    std::shared_ptr<arrow::Int32Array> fs;
    std::shared_ptr<arrow::Int32Array> tz;
 };

/**
 * Represents a column of the result set in Arrow format.
 */
struct ArrowColumn
{
    // Constructor.
    ArrowColumn(std::shared_ptr<arrow::DataType> type)
    {
        this->type = type;
    }

    // Arrow data type of the column.
    std::shared_ptr<arrow::DataType> type;

    // The array data of the columns. Each instance of ArrowColumn should only have one populated.
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
    ArrowTimestampArray    * arrowTimestamp;
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
     * This constructor is used when the metadata is available to be passed.
     * A call to appendChunk() with the initial chunk as the argument should always follow.
     *
     * @param metadata        The column metadata retrieved from the Snowflake DB.
     * @param tzString        The time zone.
     */
    ArrowChunkIterator(SF_COLUMN_DESC * metadata, std::string tzString);

    /**
     * Destructor.
     */
    ~ArrowChunkIterator() = default;

    /**
     * Helper method to update bookkeeping members that track total counts.
     *
     * Must be called before every call to appendChunk().
     *
     * @param chunk           The given chunk.
     * @param out_colCount         The total number of columns in the chunk.
     * @param out_rowCount         The total number of rows in the chunk.
     */
    void updateCounts(
        std::vector<std::shared_ptr<arrow::RecordBatch>> * chunk,
        size_t * out_colCount,
        size_t * out_rowCount);

    /**
     * Appends the given chunk to the internal result set.
     *
     * Used in conjunction with the library functions:
     * - snowflake_execute_ex(), which fetches the initial chunk.
     * - snowflake_fetch(), which prompts the chunk downloader object
     *                      to continue fetching chunks.
     *
     * In addition, also write the updated number of columns and rows in the chunk
     * to the provided buffers.
     *
     * @param chunk                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL appendChunk(std::vector<std::shared_ptr<arrow::RecordBatch>> * chunk);

    /**
     * Gets the value of the given cell as an sf_bool.
     *
     * This API method supports conversion for the following data types:
     * - BOOLEAN
     *
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsBool(size_t colIdx, size_t rowIdx, sf_bool * out_data);

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
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt8(size_t colIdx, size_t rowIdx, int8 * out_data);

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
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt32(size_t colIdx, size_t rowIdx, int32 * out_data);

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
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt64(size_t colIdx, size_t rowIdx, int64 * out_data);

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
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint8(size_t colIdx, size_t rowIdx, uint8 * out_data);

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
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint32(size_t colIdx, size_t rowIdx, uint32 * out_data);

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
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint64(size_t colIdx, size_t rowIdx, uint64 * out_data);

    /**
     * Gets the value of the given cell as a float32.
     *
     * This API method supports conversion for the following data types:
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsFloat32(size_t colIdx, size_t rowIdx, float32 * out_data);

    /**
     * Gets the value of the given cell as a float64.
     *
     * This API method supports conversion for the following data types:
     * - DOUBLE
     * - INT8, INT16, INT32, INT64
     * - STRING
     *
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsFloat64(size_t colIdx, size_t rowIdx, float64 * out_data);

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
     * @param rowIdx               The index of the row to get.
     * @param outString            The output string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsString(
        size_t colIdx,
        size_t rowIdx,
        std::string& outString);

    /**
     * Gets the value of the given cell as a timestamp.
     *
     * This API method supports conversion for the following data types:
     * - Timestamp
     *
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsTimestamp(size_t colIdx, size_t rowIdx, SF_TIMESTAMP * out_data);

    /**
     * Writes the length of the current cell to the provided buffer.
     *
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellStrlen(size_t colIdx, size_t rowIdx, size_t * out_data);

    /**
     * Returns the Arrow data type of the column associated with the given column index.
     *
     * @param colIdx          The index to the column whose data type to retrieve.
     *
     * @return the Arrow data type of the column.
     */
    std::shared_ptr<arrow::DataType> getColumnArrowDataType(uint32 colIdx);

    /**
     * Gets the row count of the current chunk.
     *
     * Used in conjunction with the library function:
     * - snowflake_affected_rows(), which retrieves the rowcount of the current chunk.
     * - snowflake_execute_ex(), which retrieves the rowcount of the current chunk.
     */
    size_t getRowCountInChunk();

    /**
     * Indicates whether the current cell is null.
     *
     * @param colIdx               The index of the column to check.
     * @param rowIdx               The index of the row to check.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL isCellNull(size_t colIdx, size_t rowIdx, sf_bool * out_data);

protected:

    /**
     * Schema of the result set.
     */
    std::shared_ptr<arrow::Schema> m_schema;

    /**
     * Vector of ArrowColumns that represents the final, columnar result set.
     */
    std::vector<std::shared_ptr<ArrowColumn>> m_columns;

private:

    /**
     * Writes the column data at the given cell to the provided ArrowColumn buffer.
     *
     * @param colIdx          The index of the column of the cell to retrieve.
     * @param rowIdx          The index of the row of the cell to retrieve.
     * @param out_cellIdx     A buffer to write the index of the ArrowColumn that the requested cell
     *                        is located at. If the cell could not be found, then this value is -1.
     *
     * @return a pointer to the ArrowColumn object to write to.
     *         If the requested cell could not be found, then nullptr is returned.
     */
    std::shared_ptr<ArrowColumn> getColumn(size_t colIdx, size_t rowIdx, int32 * out_cellIdx);

    /**
     * Internally method to check if a cell is null.
     *
     * @param colData         The column to check.
     * @param cellIdx         The index of the column to check.
     *
     * @return true if the cell is null, otherwise false.
     */
    bool isCellNull(
        std::shared_ptr<ArrowColumn> colData,
        std::shared_ptr<arrow::DataType> arrowType,
        int32 cellIdx);

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
     * Total number of rows.
     */
    uint32 m_rowCount;

    /**
     * Total number of rows inside current record batch.
     */
    uint32 m_rowCountInBatch;

    /**
     * Total number of rows inside the current chunk being processed.
     */
    uint32 m_rowCountInChunk;

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
     * A pointer to the column metadata retrieved from the Snowflake DB.
     */
    SF_COLUMN_DESC * m_metadata;

    /**
     * The tz string to use when dealing with time or timestamp values.
     */
    std::string m_tzString;

    /**
     * Associates row indices with batch indices to allow for faster retrieval of columns.
     *
     * Key: batch index.
     * Value: max row index contained in the batch.
     *
     * For example, suppose that we have the following:
     *
     * {
     *   0: 12,
     *   1: 18,
     *   2: 28,
     *   3: 35
     * }
     *
     * Where each batch consists of 5 columns.
     *
     * Batch 0 contains columns 0 through 4, each of which contains rows 0 through 12 (total 13).
     * Batch 1 contains columns 5 through 9, each of which contains rows 13 through 18 (total 6).
     * ...and so on.
     */
    std::map<uint32, uint32> m_batchRowIndexMap;

    /**
     * Indicates if the first batch has been consumed or not.
     */
    bool m_isFirstBatch;

};

} // namespace Client
} // namespace Snowflake

#endif  // SNOWFLAKECLIENT_ARROWCHUNKITERATOR_HPP
