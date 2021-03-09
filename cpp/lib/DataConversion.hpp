/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All Rights Reserved
 */
#ifndef SNOWFLAKECLIENT_DATACONVERSION_HPP
#define SNOWFLAKECLIENT_DATACONVERSION_HPP

#include <map>
#include <memory>
#include <vector>
#include <string>

#include "snowflake/basic_types.h"
#include "snowflake/client.h"
#include "ArrowChunkIterator.hpp"


namespace Snowflake
{
namespace Client
{

/**
 * Internal enumeration over all integral types to help facilitate conversion.
 */
enum IntegerType
{
    INT8  = 0, INT16  = 1, INT32  = 2, INT64  = 3,
    UINT8 = 4, UINT16 = 5, UINT32 = 6, UINT64 = 7
};

/**
 * Internal map to get the lower limit of the associated IntegerType.
 */
static std::map<IntegerType, int64> intTypeToLowerLimit =
{
    { INT8,   CHAR_MIN },
    { INT16,  SHRT_MIN },
    { INT32,  SF_INT32_MIN },
    { INT64,  SF_INT64_MIN }
};

/**
 * Internal map to get the upper limit of the associated IntegerType.
 */
static std::map<IntegerType, uint64> intTypeToUpperLimit =
{
    { INT8,   CHAR_MAX },
    { INT16,  SHRT_MAX },
    { INT32,  SF_INT32_MAX },
    { INT64,  SF_INT64_MAX }
};

namespace Util
{

    /**
     * Helper method to allocate a char buffer to write converted Arrow data to if necessary.
     *
     * TODO: It doesn't make much sense to have a util function in a conversion file.
     *       Try to find a better place to move this to.
     *
     * @param out_data             A pointer to the buffer.
     * @param out_len              The length of the string data.
     * @param out_capacity         The capacity of the provided buffer.
     * @param len                  The true length of the string to write.
     */
    void AllocateCharBuffer(
        char ** out_data,
        size_t * out_len,
        size_t * out_capacity,
        size_t len);

}

/**
 * Namespace capturing all data conversion functions.
 */
namespace Conversion
{

namespace Arrow
{

    // Numeric conversion ==========================================================================

    /**
     * Function to convert a numeric value into a signed integral value.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted integral value.
     * @param intType              The type of the buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL NumericToSignedInteger(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        int64 * out_data,
        IntegerType intType);

    /**
     * Function to convert a numeric value into an unsigned integral value.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted integral value.
     * @param intType              The type of the buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL NumericToUnsignedInteger(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        uint64 * out_data,
        IntegerType intType);

    /**
     * Function to convert a numeric value into a float64 value.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted integral value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL NumericToDouble(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        float64 * out_data);

    /**
     * Function to convert a numeric value into a float32 value.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted integral value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL NumericToFloat(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        float32 * out_data);

    /**
     * Function to convert a string value into a signed integral value.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted integral value.
     * @param intType              The type of the buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL StringToSignedInteger(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        int64 * out_data,
        IntegerType intType);

    /**
     * Function to convert a string value into an unsigned integral value.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted integral value.
     * @param intType              The type of the buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL StringToUnsignedInteger(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        uint64 * out_data,
        IntegerType intType);

    /**
     * Function to convert a string value into a float64 value.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted integral value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL StringToDouble(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        float64 * out_data);

    /**
     * Function to convert a string value into a float32 value.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted integral value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL StringToFloat(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        float32 * out_data);

    // Const String conversion =====================================================================

    /**
     * Function to convert a binary value into a const string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL BinaryToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Function to convert a boolean value into a const string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL BoolToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Function to convert a date value into a const string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL DateToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Function to convert a decimal value into a const string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL DecimalToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Function to convert a double value into a const string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL DoubleToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Function to convert an integral value into a const string.
     *
     * Note: INT32 and INT64 values may contain Snowflake TIME values.
     *       This method checks the Snowflake DB type to cover these cases.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param snowType             The Snowflake DB type of the column.
     * @param scale                The scale of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL IntToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        SF_DB_TYPE snowType,
        int64 scale,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Function to convert a time value into a const string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param scale                The scale of the time value.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL TimeToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        int64 scale,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Function to convert a time or timestamp value into a const string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param snowType             The Snowflake DB type of the column.
     * @param scale                The scale of the timestamp value.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL TimestampToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        SF_DB_TYPE snowType,
        int64 scale,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    // String conversion ===========================================================================

    /**
     * Function to convert a binary value into a string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL BinaryToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Function to convert a boolean value into a string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL BoolToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Function to convert a date value into a string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL DateToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Function to convert a decimal value into a string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL DecimalToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Function to convert a double value into a string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL DoubleToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Function to convert an integral value into a string.
     *
     * Note: INT32 and INT64 values may contain Snowflake TIME values.
     *       This method checks the Snowflake DB type to cover these cases.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param snowType             The Snowflake DB type of the column.
     * @param scale                The scale of the column.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL IntToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        SF_DB_TYPE snowType,
        int64 scale,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Function to convert a time value into a string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param scale                The scale of the time value.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL TimeToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        int64 scale,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Function to convert a time or timestamp value into a string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param arrowType            The Arrow data type of the column.
     * @param snowType             The Snowflake DB type of the column.
     * @param scale                The scale of the timestamp value.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL TimestampToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        SF_DB_TYPE snowType,
        int64 scale,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

} // namespace Arrow

namespace Json
{

    /**
     * Helper method to convert a boolean value into a proper string.
     *
     * @param value                The initial boolean value retrieved from Snowflake.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return -1 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL
    BoolToString(char * value, char ** out_data, size_t * io_len, size_t * io_capacity);

    /**
     * Helper method to convert a date value into a proper string.
     *
     * @param value                The initial date value retrieved from Snowflake.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return -1 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL
    DateToString(char * value, char ** out_data, size_t * io_len, size_t * io_capacity);

    /**
     * Helper method to convert a time or timestamp value into a proper string.
     *
     * @param value                The initial time or timestamp value retrieved from Snowflake.
     * @param scale                The scale of the time or timestamp value.
     * @param snowType             The Snowflake DB type of the time or timestamp value.
     * @param tzString             The time zone.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return -1 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL
    TimeToString(
        char * value,
        int64 scale,
        SF_DB_TYPE snowType,
        std::string tzString,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

} // namespace Json

} // namespace Conversion

} // namespace Client
} // namespace Snowflake

#endif  // SNOWFLAKECLIENT_ARROWCHUNKITERATOR_HPP
