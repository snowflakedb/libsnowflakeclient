/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_RESULTSETARROW_HPP
#define SNOWFLAKECLIENT_RESULTSETARROW_HPP

#include <string>
#include <vector>

#include "arrow/api.h"
#include "arrow/io/api.h"
#include "arrow/ipc/api.h"
#include "arrow/util/base64.h"

#include "cJSON.h"
#include "snowflake/client.h"
#include "ArrowChunkIterator.hpp"
#include "ResultSet.hpp"


namespace Snowflake
{
namespace Client
{

/**
 * Represents a result set retrieved in Arrow format.
 *
 * Internally, the result set is stored as an Arrow RecordBatch object.
 * The RecordBatch stores results as a collection of columns, as Arrays.
 *
 * It also contains a Schema object, which describes the data types associated
 * with each column.
 *
 * Unlike ResultSetJson, which internally stores its data as a collection
 * of rows, this class internally stores the result set as a colllection
 * of columns.
 *
 * When using this class, first populate the result set by using the client library
 * functions in conjunction with the appendChunk() and finishResultSet() methods.
 *
 * When the result set is finalized, consume the results by using the nextColumn()
 * and nextRow() methods to advance the internal iterators and retrieve data at
 * particular cells using the numerous getters.
 */
class ResultSetArrow : public Snowflake::Client::ResultSet
{
public:
    /**
     * Default constructor.
     */
    ResultSetArrow();

    /**
     * Parameterized constructor.
     *
     * This constructor will initialize m_records with the (partial) results
     * contained in the initial chunk. It will also initialize m_metadata with
     * the metadata in "metadata".
     *
     * @param initialChunk         A pointer to the JSON array containing result
     *                               set data of the initial chunk.
     * @param metadata             An array of metadata objects for each column.
     * @param tzString             The time zone.
     */
    ResultSetArrow(cJSON * initialChunk, SF_COLUMN_DESC * metadata, std::string tzString);

    /**
     * Destructor.
     */
    ~ResultSetArrow() = default;

    /**
     * Appends the given chunk to the internal result set.
     *
     * @param chunk                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL appendChunk(cJSON * chunk);

    /**
     * Resets the internal indices so that they may be used to traverse
     * the finished result set for consumption.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL finishResultSet();

    /**
     * Advances to the next column.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL nextColumn();

    /**
     * Advances to the next row, moving over to the next chunk if necessary.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL nextRow();

    /**
     * Writes the value of the current cell as a boolean to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsBool(sf_bool * out_data);

    /**
     * Writes the value of the current cell as an int8 to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsInt8(int8 * out_data);

    /**
     * Writes the value of the current cell as an int32 to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsInt32(int32 * out_data);

    /**
     * Writes the value of the current cell as an int64 to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsInt64(int64 * out_data);

    /**
     * Writes the value of the current cell as a uint8 to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsUint8(uint8 * out_data);

    /**
     * Writes the value of the current cell as a uint32 to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsUint32(uint32 * out_data);

    /**
     * Writes the value of the current cell as a uint64 to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsUint64(uint64 * out_data);

    /**
     * Writes the value of the current cell as a float64 to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsFloat64(float64 * out_data);

    /**
     * Writes the value of the current cell as a constant C-string to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsConstString(const char ** out_data);

    /**
     * Writes the value of the current cell as a C-string to the provided buffer.
     *
     * In the event that the provided buffer is not large enough to contain the requested string,
     * the buffer will be re-allocated and io_capacity will be updated accordingly.
     *
     * @param out_data             The buffer to write to.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsString(char * out_data, size_t * io_len, size_t * io_capacity);

    /**
     * Writes the value of the current cell as a timestamp to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsTimestamp(SF_TIMESTAMP * out_data);

    /**
     * Gets the total number of rows in the current chunk being processed.
     *
     * @return the total number of rows in the current chunk.
     */
    size_t getRowCountInChunk();

private:

    /**
     * Helper method to initialize the result set data.
     *
     * Populates m_records with the results in the first chunk, passed in as an
     * argument to the constructor.
     *
     * @param initialChunk         The first chunk of the result set.
     */
    void init(cJSON * initialChunk);

    /**
     * BufferBuilder object used to consume chunks as they are retrieved from
     * the server and continually build the final columnar result set.
     */
    std::shared_ptr<arrow::BufferBuilder> m_bufferBuilder;

    /**
     * BufferReader object used to read the chunk content in m_bufferBuilder.
     * It is used in conjunction with m_recordBatchReader to iterate over the
     * batches retrieved from the server to build the final columnar result set.
     */
    std::shared_ptr<arrow::io::BufferReader> m_bufferReader;

    /**
     * RecordBatchReader object used to operate on chunk content.
     */
    std::shared_ptr<arrow::ipc::RecordBatchReader> m_batchReader;

    /**
     * The Arrow-format chunk iterator object.
     */
    std::shared_ptr<Snowflake::Client::ArrowChunkIterator> m_chunkIterator;
};

} // namespace Client
} // namespace Snowflake

#endif // SNOWFLAKECLIENT_RESULTSETARROW_HPP
