/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_RESULTSETARROW_HPP
#define SNOWFLAKECLIENT_RESULTSETARROW_HPP

#include "arrowheaders.hpp"

#include <string>
#include <vector>

#include "cJSON.h"
#include "snowflake/client.h"
#include "ArrowChunkIterator.hpp"
#include "ResultSet.hpp"

#ifndef SF_WIN32

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
 * functions in conjunction with the appendChunk() method.
 *
 * Consume the results by using the next() method to advance the internal iterators
 * and retrieve data at particular cells using the numerous getters.
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
     * @param initialChunk         A pointer to arrow::BufferBuilder containing result
     *                               set data of the initial chunk.
     * @param metadata             An array of metadata objects for each column.
     * @param tzString             The time zone.
     */
    ResultSetArrow(arrow::BufferBuilder * initialChunk, SF_COLUMN_DESC * metadata, const std::string& tzString);


    /**
     * Parameterized constructor.
     *
     * This constructor will initialize m_records with the (partial) results
     * contained in the initial chunk. It will also initialize m_metadata with
     * the metadata in "metadata".
     *
     * @param jsonRowset64         A pointer to the rowset64 data in json result set.
     * @param metadata             An array of metadata objects for each column.
     * @param tzString             The time zone.
     */
    ResultSetArrow(cJSON* jsonRowset64, SF_COLUMN_DESC* metadata, const std::string& tzString);


    /**
     * Parameterized constructor.
     *
     * This constructor will initialize m_records with the (partial) results
     * contained in the initial chunk. It will also initialize m_metadata with
     * the metadata in "metadata".
     *
     * @param jsonRowset64         A pointer to the rowset64 data in json result set.
     * @param metadata             An array of metadata objects for each column.
     * @param tzString             The time zone.
     */
    ResultSetArrow(cJSON* jsonRowset64, SF_COLUMN_DESC* metadata, std::string tzString);

    /**
     * Destructor.
     */
    ~ResultSetArrow();

    // API methods =================================================================================

    /**
     * Appends the given chunk to the internal result set.
     *
     * @param chunkPtr                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL appendChunk(void* chunkPtr);

    /**
     * Advances the internal iterator to the next row.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL next();

    /**
     * Writes the value of the given cell as a boolean to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsBool(size_t idx, sf_bool * out_data);

    /**
     * Writes the value of the given cell as an int8 to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt8(size_t idx, int8 * out_data);

    /**
     * Writes the value of the given cell as an int32 to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt32(size_t idx, int32 * out_data);

    /**
     * Writes the value of the given cell as an int64 to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt64(size_t idx, int64 * out_data);

    /**
     * Writes the value of the given cell as a uint8 to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint8(size_t idx, uint8 * out_data);

    /**
     * Writes the value of the given cell as a uint32 to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint32(size_t idx, uint32 * out_data);

    /**
     * Writes the value of the given cell as a uint64 to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint64(size_t idx, uint64 * out_data);

    /**
     * Writes the value of the given cell as a float32 to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsFloat32(size_t idx, float32 * out_data);

    /**
     * Writes the value of the given cell as a float64 to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsFloat64(size_t idx, float64 * out_data);

    /**
     * Writes the value of the given cell as a constant C-string to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsConstString(size_t idx, const char ** out_data);

    /**
     * Writes the value of the given cell as a timestamp to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data);

    /**
     * Writes the length of the given cell to the provided buffer.
     *
     * @param idx                  The index of the row to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellStrlen(size_t idx, size_t * out_data);

    /**
     * Gets the total number of rows in the current chunk being processed.
     *
     * @return the total number of rows in the current chunk.
     */
    size_t getRowCountInChunk();

    /**
     * Indicates whether the given cell is null.
     *
     * @param idx                  The index of the row to check is null.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL isCellNull(size_t idx, sf_bool * out_data);

private:

    /**
     * The Arrow-format chunk iterator object.
     */
    std::shared_ptr<Snowflake::Client::ArrowChunkIterator> m_chunkIterator;

    /**
    * The cache for string value for each column of current row.
    */
    std::vector<std::pair<bool, std::string> > m_cacheStrVal;
};

} // namespace Client
} // namespace Snowflake

#endif // SF_WIN32
#endif // SNOWFLAKECLIENT_RESULTSETARROW_HPP
