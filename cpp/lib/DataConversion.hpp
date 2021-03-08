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

/**
 * A utility class with static member methods to perform data conversions.
 */
class DataConversions
{
public:

    // Numeric conversion ==========================================================================

    /**
     * Static method to convert a numeric value into a signed integral value.
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
    static SF_STATUS STDCALL convertNumericToSignedInteger(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        int64 * out_data,
        IntegerType intType);

    /**
     * Static method to convert a numeric value into an unsigned integral value.
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
    static SF_STATUS STDCALL convertNumericToUnsignedInteger(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        uint64 * out_data,
        IntegerType intType);

    /**
     * Static method to convert a numeric value into a float64 value.
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
    static SF_STATUS STDCALL convertNumericToDouble(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        float64 * out_data);

    /**
     * Static method to convert a numeric value into a float32 value.
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
    static SF_STATUS STDCALL convertNumericToFloat(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        float32 * out_data);

    /**
     * Static method to convert a string value into a signed integral value.
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
    static SF_STATUS STDCALL convertStringToSignedInteger(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        int64 * out_data,
        IntegerType intType);

    /**
     * Static method to convert a string value into an unsigned integral value.
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
    static SF_STATUS STDCALL convertStringToUnsignedInteger(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        uint64 * out_data,
        IntegerType intType);

    /**
     * Static method to convert a string value into a float64 value.
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
    static SF_STATUS STDCALL convertStringToDouble(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        float64 * out_data);

    /**
     * Static method to convert a string value into a float32 value.
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
    static SF_STATUS STDCALL convertStringToFloat(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        float32 * out_data);

    // Const String conversion =====================================================================

    /**
     * Static method to convert a binary value into a const string.
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
    static SF_STATUS STDCALL convertBinaryToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Static method to convert a boolean value into a const string.
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
    static SF_STATUS STDCALL convertBoolToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Static method to convert a date value into a const string.
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
    static SF_STATUS STDCALL convertDateToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Static method to convert a decimal value into a const string.
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
    static SF_STATUS STDCALL convertDecimalToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Static method to convert a double value into a const string.
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
    static SF_STATUS STDCALL convertDoubleToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Static method to convert an integral value into a const string.
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
    static SF_STATUS STDCALL convertIntToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        SF_DB_TYPE snowType,
        int64 scale,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    /**
     * Static method to convert a time value into a const string.
     *
     * @param timeSinceMidnight    The amount of time elapsed since midnight.
     * @param scale                The scale of the time value.
     * @param out_data             The buffer to which to write the converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    static SF_STATUS STDCALL convertTimeToConstString(
        int64 timeSinceMidnight,
        int64 scale,
        const char ** out_data);

    /**
     * Static method to convert a time or timestamp value into a const string.
     *
     * @param colData              The ArrowColumn for the source data.
     * @param snowType             The Snowflake DB type of the column.
     * @param scale                The scale of the timestamp value.
     * @param colIdx               The index of the column to get.
     * @param rowIdx               The index of the row to get.
     * @param cellIdx              The index of the cell to get.
     * @param out_data             The buffer to which to write the converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    static SF_STATUS STDCALL convertTimestampToConstString(
        std::shared_ptr<ArrowColumn> & colData,
        SF_DB_TYPE snowType,
        int64 scale,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        const char ** out_data);

    // String conversion ===========================================================================

    /**
     * Helper method to allocate a char buffer to write converted Arrow data to if necessary.
     *
     * TODO: It doesn't make much sense to have this as a static method of a conversion class.
     *       Try to find a better place to move this to.
     *
     * @param out_data             A pointer to the buffer.
     * @param out_len              The length of the string data.
     * @param out_capacity         The capacity of the provided buffer.
     * @param len                  The true length of the string to write.
     */
    static void allocateCharBuffer(
        char ** out_data,
        size_t * out_len,
        size_t * out_capacity,
        size_t len);

    /**
     * Static method to convert a binary value into a string.
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
    static SF_STATUS STDCALL convertBinaryToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Static method to convert a boolean value into a string.
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
    static SF_STATUS STDCALL convertBoolToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Static method to convert a date value into a string.
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
    static SF_STATUS STDCALL convertDateToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Static method to convert a decimal value into a string.
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
    static SF_STATUS STDCALL convertDecimalToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Static method to convert a double value into a string.
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
    static SF_STATUS STDCALL convertDoubleToString(
        std::shared_ptr<ArrowColumn> & colData,
        std::shared_ptr<arrow::DataType> arrowType,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Static method to convert an integral value into a string.
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
    static SF_STATUS STDCALL convertIntToString(
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
     * Static method to convert a time value into a string.
     *
     * @param timeSinceMidnight    The amount of time elapsed since midnight.
     * @param scale                The scale of the time value.
     * @param out_data             The buffer to which to write the converted string value.
     * @param io_len               The length of the string.
     * @param io_capacity          The capacity of the provided buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    static SF_STATUS STDCALL convertTimeToString(
        int64 timeSinceMidnight,
        int64 scale,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

    /**
     * Static method to convert a time or timestamp value into a string.
     *
     * @param colData              The ArrowColumn for the source data.
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
    static SF_STATUS STDCALL convertTimestampToString(
        std::shared_ptr<ArrowColumn> & colData,
        SF_DB_TYPE snowType,
        int64 scale,
        uint32 colIdx,
        uint32 rowIdx,
        uint32 cellIdx,
        char ** out_data,
        size_t * io_len,
        size_t * io_capacity);

};

} // namespace Client
} // namespace Snowflake

#endif  // SNOWFLAKECLIENT_ARROWCHUNKITERATOR_HPP
