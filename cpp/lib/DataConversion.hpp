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
    { INT64,  SF_INT64_MIN },
    { UINT8,  CHAR_MIN },
    { UINT16, SHRT_MIN },
    { UINT32, SF_INT32_MIN },
    { UINT64, SF_INT64_MIN }
};

/**
 * Internal map to get the upper limit of the associated IntegerType.
 */
static std::map<IntegerType, uint64> intTypeToUpperLimit =
{
    { INT8,   CHAR_MAX },
    { INT16,  SHRT_MAX },
    { INT32,  SF_INT32_MAX },
    { INT64,  SF_INT64_MAX },
    { UINT8,  UCHAR_MAX },
    { UINT16, USHRT_MAX },
    { UINT32, SF_UINT32_MAX },
    { UINT64, SF_UINT64_MAX }
};

const int64 power10[10] = {
    1,
    10,
    100,
    1000,
    10000,
    100000,
    1000000,
    10000000,
    100000000,
    1000000000
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
     * Function to convert between integral value.
     *
     * @param in_data              The data needs to be converted
     * @param out_data             The buffer to which to write the converted integral value.
     * @param intType              The type of the buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL IntegerToInteger(
        int64 in_data,
        int64 * out_data,
        IntegerType intType);

    /**
     * Function to convert a string value into a integral value.
     *
     * @param str_data             The string needs to be converted
     * @param out_data             The buffer to which to write the converted integral value.
     * @param intType              The type of the buffer.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL StringToInteger(
        const std::string& str_data,
        int64 * out_data,
        IntegerType intType);

    /**
    * Function to convert a string value into a uint64 value.
    *
    * @param str_data             The string needs to be converted
    * @param out_data             The buffer to which to write the converted value.
    *
    * @return 0 if successful, otherwise an error is returned.
    */
    SF_STATUS STDCALL StringToUint64(
        const std::string& str_data,
        uint64 * out_data);

    /**
     * Function to convert a string value into a float64 value.
     *
     * @param str_data             The string value needs to be converted.
     * @param out_data             The buffer to which to write the converted double value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL StringToDouble(
        const std::string& str_data,
        float64 * out_data);

    /**
     * Function to convert a string value into a float32 value.
     *
     * @param str_data             The string value needs to be converted.
     * @param out_data             The buffer to which to write the converted float value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL StringToFloat(
        const std::string& str_data,
        float32 * out_data);

    /**
     * Function to convert a date value into a string.
     *
     * @param date                 Number of days elapsed since Epoch time.
     * @param outString            The converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL DateToString(
        int64 date,
        std::string& outString);

    /**
     * Function to convert a time value into a string.
     *
     * @param timeSinceMidnight    The time data needs to be converted.
     * @param scale                The scale of the time value.
     * @param outString            The converted string value.
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL TimeToString(
        int64 timeSinceMidnight,
        int64 scale,
        std::string& outString);

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
