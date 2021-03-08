/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_RESULTSETJSON_HPP
#define SNOWFLAKECLIENT_RESULTSETJSON_HPP

#include "cJSON.h"
#include "ResultSet.hpp"

namespace Snowflake
{
namespace Client
{

/**
 * Represents a result set retrieved in JSON format.
 *
 * Internally, the result set is stored as a cJSON structure.
 *
 * When using this class, first populate the result set by using the client library
 * functions in conjunction with the appendChunk() and finishResultSet() methods.
 *
 * When the result set is finalized, consume the results by using the nextColumn()
 * and nextRow() methods to advance the internal iterators and retrieve data at
 * particular cells using the numerous getters.
 */
class ResultSetJson : public Snowflake::Client::ResultSet
{
public:

    /**
     * Default constructor.
     */
    ResultSetJson();

    /**
     * Parameterized constructor.
     *
     * @param initialChunk              A pointer to the JSON array containing result set data.
     * @param metadata                  A pointer to the metadata of the result set.
     * @param tzString                  The time zone.
     */
    ResultSetJson(cJSON * initialChunk, SF_COLUMN_DESC * metadata, std::string tzString);

    /**
     * Destructor.
     */
    ~ResultSetJson();

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
     * Advances the internal iterators to the next cell.
     *
     * Note that JSON data is stored row-wise, as retrieved.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL next();

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
     * Writes the value of the current cell as a float32 to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsFloat32(float32 * out_data);

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
    SF_STATUS STDCALL getCurrCellAsString(char ** out_data, size_t * io_len, size_t * io_capacity);

    /**
     * Writes the value of the current cell as a timestamp to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellAsTimestamp(SF_TIMESTAMP * out_data);

    /**
     * Writes the length of the current cell to the provided buffer.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCurrCellStrlen(size_t * out_data);

    /**
     * Gets the current row.
     *
     * Used in the client function snowflake_fetch() when updating the statement
     * handle object with the current row. Only for JSON query format.
     *
     * @return the current row.
     */
    cJSON * getCurrRow();

    /**
     * Gets the total number of rows in the current chunk being processed.
     *
     * @return the number of rows in the current chunk.
     */
    size_t getRowCountInChunk();

    /**
     * Indicates whether the current cell is null.
     *
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL isCurrCellNull(sf_bool * out_data);

private:

    /**
     * Helper method to retrieve the value at a given cell.
     *
     * @param rowIdx               The row index of the cell to retrieve.
     * @param colIdx               The column index of the cell to retrieve.
     *
     * @return the cJSON value of the cell, or NULL if unsuccessful.
     */
    cJSON * getCellValue(uint32 rowIdx, uint32 colIdx);

    /**
     * The JSON array representing the result set.
     */
    cJSON * m_records;

    /**
     * The number of rows in the chunk currently being processed.
     */
    size_t m_rowCountInChunk;
};

} // namespace Client
} // namespace Snowflake

#endif // SNOWFLAKECLIENT_RESULTSETJSON_HPP
