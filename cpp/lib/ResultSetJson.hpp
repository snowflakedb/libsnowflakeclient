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
 * Internally, the result set is stored as a cJSON structure representing the current chunk being
 * processed. This chunk will be continually freed and set as the application consumes chunks.
 *
 * When using this class, populate the result set one chunk at a time by using the client library
 * function, snowflake_fetch(), which will call appendChunk(). This sets the chunk to the provided
 * chunk and sets the result set in its initial state.
 *
 * Then, snowflake_fetch() will start the consumption of the chunk by calling the next() method.
 * Initially, the iterator is set to a null state. The first call will move the iterator to the
 * first row of the chunk.
 *
 * From there, you can retrieve data at particular cells using the numerous getter functions.
 * To advance to the next row, simply call snowflake_fetch() again, which will in turn call
 * next() or appendChunk() if there are no more rows to be consumed.
 *
 * Note: We cache both the current row for performance.
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
     * @param rowset                    A pointer to the JSON array containing result set data.
     * @param metadata                  A pointer to the metadata of the result set.
     * @param tzString                  The time zone.
     */
    ResultSetJson(
        cJSON * rowset,
        SF_COLUMN_DESC * metadata,
        const std::string& tzString);

    /**
     * Destructor.
     */
    ~ResultSetJson();

    /**
     * Frees the previously held chunk in m_chunk and sets the current chunk to the given chunk.
     *
     * @param chunk                The chunk to append.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL appendChunk(void * chunkPtr);

    /**
     * Advances the internal iterator to the next row. If there are no more rows to consume,
     * then the position is maintained until it is reset to the initial state by appendChunk().
     *
     * Note: The initial state of iterator is nullptr. 
     *       The first call to next() will set the iterator to the first row of the chunk.
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
    SF_STATUS STDCALL getCellAsBool(size_t idx, sf_bool * out_data);

    /**
     * Writes the value of the given cell as an int8 to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt8(size_t idx, int8 * out_data);

    /**
     * Writes the value of the given cell as an int32 to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt32(size_t idx, int32 * out_data);

    /**
     * Writes the value of the given cell as an int64 to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsInt64(size_t idx, int64 * out_data);

    /**
     * Writes the value of the given cell as a uint8 to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint8(size_t idx, uint8 * out_data);

    /**
     * Writes the value of the given cell as a uint32 to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint32(size_t idx, uint32 * out_data);

    /**
     * Writes the value of the given cell as a uint64 to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsUint64(size_t idx, uint64 * out_data);

    /**
     * Writes the value of the given cell as a float32 to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsFloat32(size_t idx, float32 * out_data);

    /**
     * Writes the value of the given cell as a float64 to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsFloat64(size_t idx, float64 * out_data);

    /**
     * Writes the value of the given cell as a constant C-string to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsConstString(size_t idx, const char ** out_data);

    /**
     * Writes the value of the given cell as a timestamp to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellAsTimestamp(size_t idx, SF_TIMESTAMP * out_data);

    /**
     * Writes the length of the given cell to the provided buffer.
     *
     * @param idx                  The index of the column to retrieve.
     * @param out_data             The buffer to write to.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL getCellStrlen(size_t idx, size_t * out_data);

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
    SF_STATUS STDCALL isCellNull(size_t idx, sf_bool * out_data);

private:

    /**
     * The current chunk retrieved from the server.
     */
    cJSON * m_chunk;

    /**
     * The JSON array representing the current row.
     */
    cJSON * m_currRow;

    /**
     * The number of rows in the chunk currently being processed.
     */
    size_t m_rowCountInChunk;
};

} // namespace Client
} // namespace Snowflake

#endif // SNOWFLAKECLIENT_RESULTSETJSON_HPP
