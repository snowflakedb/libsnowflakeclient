/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_RESULTSETARROW_HPP
#define SNOWFLAKECLIENT_RESULTSETARROW_HPP

#include <string>
#include <vector>

#include "arrow/api.h"

#include "client.h"
#include "logger.h"
#include "snowflake/ResultSet.hpp"


namespace Snowflake
{
namespace Client
{

/**
 * A custom struct to represent Timestamp values.
 */
struct ArrowTimestamp
{
    int64 secondsSinceEpoch = 0;
    int32 fracSeconds = 0;
    int32 timeOffset = 0;
};

/**
 * Represents a single column of a result set in Arrow format.
 */
union ArrowColumn
{
    arrow::BinaryArray     a_binary;
    arrow::BooleanArray    a_boolean;
    arrow::Date32Array     a_date32;
    arrow::Date64Array     a_date64;
    arrow::DoubleArray     a_double;
    arrow::Int8Array       a_int8;
    arrow::Int16Array      a_int16;
    arrow::Int32Array      a_int32;
    arrow::Int64Array      a_int64;
    arrow::Time64Array     a_time64;
    arrow::TimestampArray  a_timestamp;
};

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
 * of columns. However, it exposes methods to traverse the result set as
 * if it were split into rows.
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
     * This constructor will initialize the m_records with the (partial) results
     * contained in "data". It will also initialize m_metadata withthe metadata
     * in "metadata".
     * 
     * It will then utilize the given chunk downloader instance to retrieve the
     * remaining data and store it in m_records.
     *
     * This class takes ownership of the "chunkDownloader" pointer.
     * After initialization has been completed, the pointer will be deleted.
     *
     * @param data                 A pointer to the JSON array containing result
     *                               set data.
     * @param metadata             An array of metadata objects for each column.
     * @param chunkDownloader      A pointer to the chunk downloader object.
     * @param queryId              The query ID.
     * @param tzOffset             The time zone offset.
     * @param totalChunkCount      The total number of chunks.
     * @param totalColumnCount     The total number of columns.
     * @param totalRowCount        The total number of rows.
     */
    ResultSetArrow(
        cJSON * data,
        SF_COLUMN_DESC * metadata,
        SF_CHUNK_DOWNLOADER * chunkDownloader,
        std::string queryId,
        int32 tzOffset,
        size_t totalChunkCount,
        size_t totalColumnCount,
        size_t totalRowCount
    );

    /**
     * Destructor.
     */
    ~ResultSetArrow();

    /**
     * Get the value of the current column as a string.
     *
     * @return The current column as a string.
     */
    std::string getCurrentColumnAsString();

    /**
     * Get the value of the current row as a string.
     *
     * @return The current row as a string.
     */
    std::string getCurrentRowAsString();

    /**
     * Advance to the next column.
     *
     * @return true if the next column exists, otherwise false.
     */
    bool nextColumn();

    /**
     * Advance to the next row, moving over to the next chunk if necessary.
     *
     * @return true if the next row exists, otherwise false.
     */
    bool nextRow();

private:

    /**
     * Helper method to initialize the result set data.
     *
     * Populates m_records with the results in the first chunk, passed in as an
     * argument to the constructor, and any subsequent chunks by using
     * m_chunkDownloader to eagerly fetch rows.
     *
     * @param data                 The first chunk of the result set as cJSON.
     */
    void init(cJSON * data);

    /**
     * Helper method to initialize the table schema.
     *
     * The schema is a collection of equal-length arrow::Field instances.
     */
    std::shared_ptr<arrow::Schema> initSchema();

    /**
     * Helper method to initialize the table columns.
     *
     * @param data                 The current chunk as a cJSON array.
     *
     * @return the vector of Arrow columns.
     */
    std::vector<std::shared_ptr<arrow::Array>> initColumns(cJSON * data);

    /**
     * Helper method to initialize the arrow::ArrayBuilder instances that will
     * build the arrays representing the columns of the table.
     *
     * @param chunk                The current chunk as a cJSON array.
     *
     * @return the vector of pointers to ArrayBuilders.
     */
    std::vector<arrow::ArrayBuilder *> initBuilders(cJSON * chunk);

    /**
     * Helper method to create an approriately-typed arrow::Field object,
     * given the Snowflake DB type of the column to build.
     *
     * If you change the mapping of this method, you must also change
     * ResultSetArrow::convertTypeFromSnowflakeToArrow() to reflect these changes.
     *
     * @param snowflakeType        The Snowflake DB type of the column to build.
     * @param name                 The name of the column.
     * @param scale                The scale of the column.
     *
     * @return the field object.
     *
     * @see lib/results.c
     */
    std::shared_ptr<arrow::Field> createArrowField(
        SF_DB_TYPE snowflakeType,
        std::string name,
        int64 scale);

    /**
     * Helper method to create an approriately-typed arrow::ArrayBuilder object,
     * given the Snowflake DB type of the column to build.
     *
     * Returns raw pointer due to compile-time errors with smart pointers.
     *
     * If you change the mapping of this method, you must also change
     * ResultSetArrow::convertTypeFromSnowflakeToArrow() to reflect these changes.
     *
     * @param snowflakeType        The Snowflake DB type of the column to build.
     * @param scale                The scale of the column.
     *
     * @return a raw pointer to the array builder object.
     *
     * @see lib/results.c
     */
    arrow::ArrayBuilder * createArrowArrayBuilder(
        SF_DB_TYPE snowflakeType,
        int64 scale);

    /**
     * Helper method to populate the given arrow::ArrayBuilder instances.
     *
     * Note: This method should only be called by the initBuilders() method.
     * In addition, m_currChunkRowIdx must be zeroed before calling.
     *
     * @param builders             The vector of ArrayBuilder pointers.
     * @param chunk                The current chunk as a cJSON array.
     */
    void appendChunkToBuilders(
        std::vector<arrow::ArrayBuilder *> builders,
        cJSON * chunk);

    /**
     * Helper method to convert a value retrieved from cJSON to an appropriate
     * C type and append the converted value to the ArrayBuilder for the current 
     * column.
     *
     * @param jsonValue                 The value as a cJSON object.
     * @param builders                  The vector of ArrayBuilders to update.
     *
     * @return The Arrow representation of the given value.
     */
    void convertAndAppendValue(
        cJSON * jsonValue,
        std::vector<arrow::ArrayBuilder *> builders);

    /**
     * Helper method to convert a Snowflake DB type to an Arrow type.
     *
     * If you change the mapping of this method, you must also change
     * ResultSetArrow::getArrayBuilder() to reflect these changes.
     *
     * @param snowflakeType        The Snowflake DB type to convert.
     * @param scale                The scale of the column.
     *
     * @return the Arrow data type.
     *
     * @see lib/results.c
     */
    std::shared_ptr<arrow::DataType>
    convertTypeFromSnowflakeToArrow(SF_DB_TYPE snowflakeType, int64 scale);

    /**
     * Helper method to convert the Timestamp value retrieved from cJSON to an
     * instance of ArrowTimestamp.
     *
     * @param ds                   The date string to convert.
     *
     * @return The resultant ArrowTimestamp instance.
     */
    int32 convertDateStringToInt32(std::string ds);

    /**
     * Helper method to convert the Timestamp value retrieved from cJSON to an
     * instance of ArrowTimestamp.
     *
     * @param ts                   The time string to convert.
     * @param scale                The scale of the time string.
     *
     * @return The resultant ArrowTimestamp instance.
     */
    int32 convertTimeStringToInt32(std::string ts, int64 scale);

    /**
     * Helper method to convert the Timestamp value retrieved from cJSON to an
     * instance of ArrowTimestamp.
     *
     * @param ts                   The time string to convert.
     * @param scale                The scale of the time string.
     *
     * @return The resultant ArrowTimestamp instance.
     */
    int64 convertTimeStringToInt64(std::string ts, int64 scale);

    /**
     * Helper method to convert the Timestamp value retrieved from cJSON to an
     * instance of ArrowTimestamp.
     *
     * @param tsCString            The timestamp as a C-style string.
     *
     * @return The resultant ArrowTimestamp instance.
     */
    ArrowTimestamp convertTimestampStringToStruct(char * tsCString);

    /**
     * Helper method to advance index values while initializing the result set.
     *
     * @return true if next row exists, false if next row does not exist.
     */
    bool advance();

    /**
     * A RecordBatch representing the result set.
     */
    std::shared_ptr<arrow::RecordBatch> m_records;

    /**
     * An array of SF_COLUMN_DESC instances, representing column metadata.
     * The size of the array is equal to m_totalColumnCount.
     */
    SF_COLUMN_DESC * m_metadata;
};

}
}

#endif // SNOWFLAKECLIENT_RESULTSETARROW_HPP
