/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All right reserved.
 */

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <string>

#include "arrow/util/basic_decimal.h"
#include "arrow/util/decimal.h"

#include "../logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "ArrowChunkIterator.hpp"
#include "DataConversion.hpp"
#include "ResultSet.hpp"


namespace Snowflake
{
namespace Client
{

// Helper methods for numeric conversion ===========================================================

SF_STATUS STDCALL DataConversions::convertNumericToSignedInteger(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    int64 * out_data,
    IntegerType intType
)
{
    // Determine upper and lower limits based on buffer type.
    int64 lowerLimit = intTypeToLowerLimit[intType];
    uint64 upperLimit = intTypeToUpperLimit[intType];

    int64 convData;
    switch (colData->type->id())
    {
        case arrow::Type::type::BOOL:
        {
            bool rawData = colData->arrowBoolean->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        case arrow::Type::type::DATE32:
        {
            int32_t rawData = colData->arrowDate32->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        case arrow::Type::type::DATE64:
        {
            int64_t rawData = colData->arrowDate64->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        case arrow::Type::type::DECIMAL:
        {
            // TODO: Consider performance.
            const uint8_t * rawData = colData->arrowDecimal128->Value(cellIdx);
            int32_t byteWidth = colData->arrowDecimal128->byte_width();
            auto basicDec128 = arrow::BasicDecimal128(rawData);
            auto dec128 = arrow::Decimal128(basicDec128);
            convData = static_cast<int64>(int64_t(dec128));
            break;
        }
        case arrow::Type::type::DOUBLE:
        {
            double rawData = colData->arrowDouble->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        case arrow::Type::type::INT8:
        {
            int8_t rawData = colData->arrowInt8->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        case arrow::Type::type::INT16:
        {
            int16_t rawData = colData->arrowInt16->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        case arrow::Type::type::INT32:
        {
            int32_t rawData = colData->arrowInt32->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        case arrow::Type::type::INT64:
        {
            int64_t rawData = colData->arrowInt64->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        default:
        {
            // Should never be reached.
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    if (convData < lowerLimit || convData > upperLimit)
    {
        CXX_LOG_ERROR("Cell at column %d, row %d out of bounds.", colIdx, rowIdx);
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertNumericToUnsignedInteger(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    uint64 * out_data,
    IntegerType intType
)
{
    // Determine upper limit based on buffer type.
    uint64 upperLimit = intTypeToUpperLimit[intType];

    uint64 convData;
    switch (colData->type->id())
    {
        case arrow::Type::type::BOOL:
        {
            bool rawData = colData->arrowBoolean->Value(cellIdx);
            convData = static_cast<int64>(rawData);
            break;
        }
        case arrow::Type::type::DATE32:
        {
            int32_t rawData = colData->arrowDate32->Value(cellIdx);
            // Given the semantics of a DATE, it doesn't make sense to convert negative values.
            if (rawData < 0)
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            convData = static_cast<uint64>(rawData);
            break;
        }
        case arrow::Type::type::DATE64:
        {
            int64_t rawData = colData->arrowDate64->Value(cellIdx);
            if (rawData < 0)
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            convData = static_cast<uint64>(rawData);
            break;
        }
        case arrow::Type::type::DECIMAL:
        {
            // TODO: Consider performance.
            const uint8_t * rawData = colData->arrowDecimal128->Value(cellIdx);
            int32_t byteWidth = colData->arrowDecimal128->byte_width();
            auto basicDec128 = arrow::BasicDecimal128(rawData);
            auto dec128 = arrow::Decimal128(basicDec128);
            convData = static_cast<uint64>(int64_t(dec128));
            break;
        }
        case arrow::Type::type::DOUBLE:
        {
            double rawData = colData->arrowDouble->Value(cellIdx);
            // Treat negative values as bad input.
            if (rawData < 0L)
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            convData = static_cast<uint64>(std::llround(rawData));
            break;
        }
        case arrow::Type::type::INT8:
        {
            int8_t rawData = colData->arrowInt8->Value(cellIdx);
            // Treat negative values as bad input.
            if (rawData < 0)
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            convData = static_cast<uint64>(rawData);
            break;
        }
        case arrow::Type::type::INT16:
        {
            int16_t rawData = colData->arrowInt16->Value(cellIdx);
            // Treat negative values as bad input.
            if (rawData < 0)
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            convData = static_cast<uint64>(rawData);
            break;
        }
        case arrow::Type::type::INT32:
        {
            int32_t rawData = colData->arrowInt32->Value(cellIdx);
            // Treat negative values as bad input.
            if (rawData < 0)
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            convData = static_cast<uint64>(rawData);
            break;
        }
        case arrow::Type::type::INT64:
        {
            int64_t rawData = colData->arrowInt64->Value(cellIdx);
            // Treat negative values as bad input.
            if (rawData < 0)
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            convData = static_cast<uint64>(rawData);
            break;
        }
        default:
        {
            // Should never be reached.
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    if (convData > upperLimit)
    {
        CXX_LOG_ERROR("Cell at column %d, row %d out of bounds.", colIdx, rowIdx);
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertNumericToDouble(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    float64 * out_data
)
{
    float64 convData;
    switch (colData->type->id())
    {
        case arrow::Type::type::DOUBLE:
        {
            convData = colData->arrowDouble->Value(cellIdx);
            break;
        }
        case arrow::Type::type::INT8:
        {
            int8_t rawData = colData->arrowInt8->Value(cellIdx);
            convData = static_cast<float64>(rawData);
            break;
        }
        case arrow::Type::type::INT16:
        {
            int16_t rawData = colData->arrowInt16->Value(cellIdx);
            convData = static_cast<float64>(rawData);
            break;
        }
        case arrow::Type::type::INT32:
        {
            int32_t rawData = colData->arrowInt32->Value(cellIdx);
            convData = static_cast<float64>(rawData);
            break;
        }
        case arrow::Type::type::INT64:
        {
            int64_t rawData = colData->arrowInt64->Value(cellIdx);
            convData = static_cast<float64>(rawData);
            break;
        }
        default:
        {
            // Should never be reached.
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertNumericToFloat(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    float32 * out_data
)
{
    float32 convData;
    switch (colData->type->id())
    {
        case arrow::Type::type::DOUBLE:
        {
            float64 rawData = colData->arrowDouble->Value(cellIdx);
            convData = static_cast<float32>(rawData);
            break;
        }
        case arrow::Type::type::INT8:
        {
            int8_t rawData = colData->arrowInt8->Value(cellIdx);
            convData = static_cast<float32>(rawData);
            break;
        }
        case arrow::Type::type::INT16:
        {
            int16_t rawData = colData->arrowInt16->Value(cellIdx);
            convData = static_cast<float32>(rawData);
            break;
        }
        case arrow::Type::type::INT32:
        {
            int32_t rawData = colData->arrowInt32->Value(cellIdx);
            convData = static_cast<float32>(rawData);
            break;
        }
        case arrow::Type::type::INT64:
        {
            int64_t rawData = colData->arrowInt64->Value(cellIdx);
            convData = static_cast<float32>(rawData);
            break;
        }
        default:
        {
            // Should never be reached.
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertStringToSignedInteger(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    int64 * out_data,
    IntegerType intType
)
{
    // Determine upper and lower limits based on buffer type.
    int64 lowerLimit = intTypeToLowerLimit[intType];
    uint64 upperLimit = intTypeToUpperLimit[intType];

    int64 convData;
    switch (colData->type->id())
    {
        case arrow::Type::type::STRING:
        {
            errno = 0;
            size_t charsProcessed;
            std::string rawData = colData->arrowString->GetString(cellIdx);
            convData = static_cast<int64>(std::stoll(rawData, &charsProcessed, 10));

            if (errno != 0)
            {
                CXX_LOG_ERROR("conversion from STRING to INTEGER failed.");
                CXX_LOG_ERROR("value %s resulted in errno %d", rawData, errno);
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            }

            break;
        }
        default:
        {
            // Should never be reached.
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    if (convData < lowerLimit || convData > upperLimit)
    {
        CXX_LOG_ERROR("Cell at column %d, row %d out of bounds.", colIdx, rowIdx);
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertStringToUnsignedInteger(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    uint64 * out_data,
    IntegerType intType
)
{
    // Determine upper limit based on buffer type.
    uint64 upperLimit = intTypeToUpperLimit[intType];

    uint64 convData;
    switch (colData->type->id())
    {
        case arrow::Type::type::STRING:
        {
            errno = 0;
            size_t charsProcessed;
            std::string rawData = colData->arrowString->GetString(cellIdx);
            convData = static_cast<uint64>(std::stoull(rawData, &charsProcessed, 10));

            if (errno != 0)
            {
                CXX_LOG_ERROR("conversion from STRING to UNSIGNED INTEGER failed.");
                CXX_LOG_ERROR("value %s resulted in errno %d", rawData, errno);
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            }

            break;
        }
        default:
        {
            // Should never be reached.
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    if (convData > upperLimit)
    {
        CXX_LOG_ERROR("Cell at column %d, row %d out of bounds.", colIdx, rowIdx);
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertStringToDouble(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    float64 * out_data
)
{
    float64 convData;
    switch (colData->type->id())
    {
        case arrow::Type::type::STRING:
        {
            errno = 0;
            size_t charsProcessed;
            std::string rawData = colData->arrowString->GetString(cellIdx);
            convData = static_cast<float64>(std::stod(rawData, &charsProcessed));

            if (errno != 0)
            {
                CXX_LOG_ERROR("conversion from STRING to DOUBLE failed.");
                CXX_LOG_ERROR("value %s resulted in errno %d", rawData, errno);
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            }

            break;
        }
        default:
        {
            // Should never be reached.
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertStringToFloat(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    float32 * out_data
)
{
    float32 convData;
    switch (colData->type->id())
    {
        case arrow::Type::type::STRING:
        {
            errno = 0;
            size_t charsProcessed;
            std::string rawData = colData->arrowString->GetString(cellIdx);
            convData = static_cast<float32>(std::stof(rawData, &charsProcessed));

            if (errno != 0)
            {
                CXX_LOG_ERROR("conversion from STRING to DOUBLE failed.");
                CXX_LOG_ERROR("value %s resulted in errno %d", rawData, errno);
                return SF_STATUS_ERROR_CONVERSION_FAILURE;
            }

            break;
        }
        default:
        {
            // Should never be reached.
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

// Helper methods for const string conversion ======================================================

SF_STATUS STDCALL DataConversions::convertBinaryToConstString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    const char ** out_data
)
{
    if (arrowType->id() != arrow::Type::type::BINARY)
    {
        CXX_LOG_ERROR("Trying to convert non-BINARY type to binary string: %d.", arrowType->id());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    std::string strValue = colData->arrowBinary->GetString(cellIdx);
    *out_data = strValue.c_str();

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertBoolToConstString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    const char ** out_data
)
{
    if (arrowType->id() != arrow::Type::type::BOOL)
    {
        CXX_LOG_ERROR("Trying to convert non-BOOL type to bool string: %d.", arrowType->id());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    bool boolValue = colData->arrowBoolean->Value(cellIdx);
    const char * strValue = (boolValue) ? SF_BOOLEAN_TRUE_STR : SF_BOOLEAN_FALSE_STR;
    *out_data = strValue;

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertDateToConstString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    const char ** out_data
)
{
    // Probably want to move these values to a better place.
    int maxDateLength = 12;
    const char * dateFormat = "%Y-%m-%d";

    // Number of days elapsed since Epoch time.
    int64 dateValue;
    switch (arrowType->id())
    {
        case arrow::Type::type::DATE32:
        {
            dateValue = static_cast<int64>(colData->arrowDate32->Value(cellIdx));
            break;
        }
        case arrow::Type::type::DATE64:
        {
            dateValue = static_cast<int64>(colData->arrowDate64->Value(cellIdx));
            break;
        }
        default:
        {
            CXX_LOG_ERROR("Trying to convert non-DATE type to date string: %d.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // The main logic for the conversion.
    struct tm tm_obj;
    struct tm *tm_ptr;
    std::memset(&tm_obj, 0, sizeof(tm_obj));
    time_t sec = (time_t) dateValue * 24L * 60L * 60L;
    tm_ptr = sf_gmtime(&sec, &tm_obj);

    // If tm_ptr is null, then the operation has failed.
    if (tm_ptr == nullptr)
    {
        if (*out_data != nullptr)
        {
            delete *out_data;
            *out_data = nullptr;
        }

        CXX_LOG_DEBUG("Failed to convert date value to string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    char * buf;
    std::strftime(buf, maxDateLength + 1, dateFormat, &tm_obj);
    *out_data = buf;

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertDecimalToConstString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    const char ** out_data
)
{
    if (arrowType->id() != arrow::Type::type::DECIMAL)
    {
        CXX_LOG_ERROR("Trying to convert non-DECIMAL type to decimal string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    std::string strValue = colData->arrowDecimal128->GetString(cellIdx);
    CXX_LOG_DEBUG("Retrieved strValue %s from colData.", strValue);
    *out_data = strValue.c_str();

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertDoubleToConstString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    const char ** out_data
)
{
    if (arrowType->id() != arrow::Type::type::DOUBLE)
    {
        CXX_LOG_ERROR("Trying to convert non-DOUBLE type to decimal string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    float64 doubleValue = colData->arrowDouble->Value(cellIdx);
    std::string strValue = std::to_string(doubleValue);
    *out_data = strValue.c_str();

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertIntToConstString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    SF_DB_TYPE snowType,
    int64 scale,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    const char ** out_data
)
{
    int64 rawData;
    switch (arrowType->id())
    {
        case arrow::Type::type::INT8:
        {
            rawData = static_cast<int64>(colData->arrowInt8->Value(cellIdx));
            break;
        }
        case arrow::Type::type::INT16:
        {
            rawData = static_cast<int64>(colData->arrowInt16->Value(cellIdx));
            break;
        }
        case arrow::Type::type::INT32:
        {
            rawData = static_cast<int64>(colData->arrowInt32->Value(cellIdx));
            break;
        }
        case arrow::Type::type::INT64:
        {
            rawData = static_cast<int64>(colData->arrowInt64->Value(cellIdx));
            break;
        }
        default:
        {
            CXX_LOG_ERROR("Trying to convert non-INT type to int string: %d.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // If the Snowflake DB type is TIME, then convert to a time string.
    if (snowType == SF_DB_TYPE_TIME)
        return convertTimeToConstString(rawData, scale, out_data);

    // Otherwise, convert integral value to string and write to buffer.
    std::string strValue = std::to_string(rawData);
    *out_data = strValue.c_str();

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertTimeToConstString(
    int64 timeSinceMidnight,
    int64 scale,
    const char ** out_data
)
{
    CXX_LOG_DEBUG("Converting time value %d with scale %d.", timeSinceMidnight, scale);
    // Construct a tm object holding appropriate time values.
    std::tm timeObj;
    timeObj.tm_mday = 1;
    int64 secsSinceMidnight = timeSinceMidnight / std::pow(10, scale);

    std::lldiv_t divBySecs = std::lldiv(secsSinceMidnight, 60);
    std::lldiv_t divByMins = std::lldiv(divBySecs.quot, 60);
    timeObj.tm_hour = divByMins.quot;
    timeObj.tm_min = divByMins.rem;
    timeObj.tm_sec = divBySecs.rem;
    CXX_LOG_DEBUG("tm_hour: %d", timeObj.tm_hour);
    CXX_LOG_DEBUG("tm_min: %d", timeObj.tm_min);
    CXX_LOG_DEBUG("tm_sec: %d", timeObj.tm_sec);

    // Create Timestamp object. This will be used to create the end time string.
    SF_TIMESTAMP ts;
    ts.tm_obj = timeObj;
    ts.nsec = timeSinceMidnight % (int64) std::pow(10, scale);
    ts.tzoffset = 0;
    ts.scale = static_cast<int32>(scale);
    ts.ts_type = SF_DB_TYPE_TIME;

    // Leverage client library to convert to string.
    // The client will handle the allocation of resources if necessary.
    char * buf;
    size_t len;
    SF_STATUS status = snowflake_timestamp_to_string(&ts, "", &buf, 0, &len, SF_BOOLEAN_TRUE);
    if (status != SF_STATUS_SUCCESS)
        return status;

    *out_data = buf;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertTimestampToConstString(
    std::shared_ptr<ArrowColumn> & colData,
    SF_DB_TYPE snowType,
    int64 scale,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    const char ** out_data
)
{
    SF_TIMESTAMP sfts;

    // The C API has a custom struct `SF_TIMESTAMP` used to represent Timestamp values.
    // In addition to what's available in the ArrowTimestamp struct, we must provide:
    // (1) A tm object that represents the timestamp value,
    // (2) The scale of the value,
    // (3) The timestamp type, i.e. LTZ, NTZ, TZ.
    ArrowTimestampArray * ts = colData->arrowTimestamp;
    time_t t = static_cast<time_t>(ts->sse->Value(cellIdx));
    sfts.tm_obj = *(std::localtime(&t));
    sfts.nsec = ts->fs->Value(cellIdx);
    sfts.tzoffset = ts->tz->Value(cellIdx);
    sfts.scale = static_cast<int64>(scale);
    sfts.ts_type = snowType;

    // Leverage client library to convert to string.
    // The client will handle the allocation of resources if necessary.
    char * buf;
    size_t len;
    SF_STATUS status = snowflake_timestamp_to_string(&sfts, "", &buf, 0, &len, SF_BOOLEAN_TRUE);
    if (status != SF_STATUS_SUCCESS)
        return status;

    *out_data = buf;
    return SF_STATUS_SUCCESS;
}

// Helper methods for string conversion ============================================================

void DataConversions::allocateCharBuffer(
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity,
    size_t len
)
{
    // Allocate pointers for length and capacity containers if necessary,
    // then initialize with appropriate values.
    if (io_len == nullptr)
        io_len = new size_t;
    *io_len = len;

    if (io_capacity == nullptr)
        io_capacity = new size_t;
    *io_capacity = len + 1;

    // Ensure that the buffer is well-allocated.
    if (*out_data == nullptr)
    {
        CXX_LOG_DEBUG("Given unallocating buffer. Allocating...");
        *out_data = new char[*io_capacity];
        CXX_LOG_DEBUG("Allocated at 0x%x with capacity %d.", *out_data, *io_capacity);
    }
}

SF_STATUS STDCALL DataConversions::convertBinaryToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    if (arrowType->id() != arrow::Type::type::BINARY)
    {
        CXX_LOG_ERROR("Trying to convert non-BINARY type to binary string: %d.", arrowType->id());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    std::string strValue = colData->arrowBinary->GetString(cellIdx);
    allocateCharBuffer(out_data, io_len, io_capacity, strValue.size());
    std::strncpy(*out_data, strValue.c_str(), *io_len);
    (*out_data)[*io_len] = '\0';

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertBoolToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    if (arrowType->id() != arrow::Type::type::BOOL)
    {
        CXX_LOG_ERROR("Trying to convert non-BOOL type to bool string: %d.", arrowType->id());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    bool boolValue = colData->arrowBoolean->Value(cellIdx);
    const char * strValue = (boolValue) ? SF_BOOLEAN_TRUE_STR : SF_BOOLEAN_FALSE_STR;
    allocateCharBuffer(out_data, io_len, io_capacity, std::strlen(strValue));
    std::strncpy(*out_data, strValue, *io_len);
    (*out_data)[*io_len] = '\0';

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertDateToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    // Probably want to move these values to a better place.
    int maxDateLength = 12;
    const char * dateFormat = "%Y-%m-%d";
    allocateCharBuffer(out_data, io_len, io_capacity, maxDateLength);

    // Number of days elapsed since Epoch time.
    int64 dateValue;
    switch (arrowType->id())
    {
        case arrow::Type::type::DATE32:
        {
            dateValue = static_cast<int64>(colData->arrowDate32->Value(cellIdx));
            break;
        }
        case arrow::Type::type::DATE64:
        {
            dateValue = static_cast<int64>(colData->arrowDate64->Value(cellIdx));
            break;
        }
        default:
        {
            CXX_LOG_ERROR("Trying to convert non-DATE type to date string: %d.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // The main logic for the conversion.
    struct tm tm_obj;
    struct tm *tm_ptr;
    std::memset(&tm_obj, 0, sizeof(tm_obj));
    time_t sec = (time_t) dateValue * 24L * 60L * 60L;
    tm_ptr = sf_gmtime(&sec, &tm_obj);

    // If tm_ptr is null, then the operation has failed.
    if (tm_ptr == nullptr)
    {
        if (*out_data != nullptr)
        {
            delete *out_data;
            *out_data = nullptr;
        }

        if (io_len != nullptr)
        {
            delete io_len;
            io_len = nullptr;
        }

        if (io_capacity != nullptr)
        {
            delete io_capacity;
            io_capacity = nullptr;
        }

        CXX_LOG_DEBUG("Failed to convert date value to string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    std::strftime(*out_data, *io_capacity, dateFormat, &tm_obj);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertDecimalToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    if (arrowType->id() != arrow::Type::type::DECIMAL)
    {
        CXX_LOG_ERROR("Trying to convert non-DECIMAL type to decimal string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    std::string strValue = colData->arrowDecimal128->GetString(cellIdx);
    CXX_LOG_DEBUG("Retrieved strValue %s from colData.", strValue);
    allocateCharBuffer(out_data, io_len, io_capacity, strValue.size());
    std::strncpy(*out_data, strValue.c_str(), *io_len);
    (*out_data)[*io_len] = '\0';

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertDoubleToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    if (arrowType->id() != arrow::Type::type::DOUBLE)
    {
        CXX_LOG_ERROR("Trying to convert non-DOUBLE type to decimal string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    float64 doubleValue = colData->arrowDouble->Value(cellIdx);
    std::string strValue = std::to_string(doubleValue);
    allocateCharBuffer(out_data, io_len, io_capacity, strValue.size());
    CXX_LOG_DEBUG("len %d, capacity %d", *io_len, *io_capacity);
    std::strncpy(*out_data, strValue.c_str(), *io_len);
    (*out_data)[*io_len] = '\0';

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertIntToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    SF_DB_TYPE snowType,
    int64 scale,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    int64 rawData;
    switch (arrowType->id())
    {
        case arrow::Type::type::INT8:
        {
            rawData = static_cast<int64>(colData->arrowInt8->Value(cellIdx));
            break;
        }
        case arrow::Type::type::INT16:
        {
            rawData = static_cast<int64>(colData->arrowInt16->Value(cellIdx));
            break;
        }
        case arrow::Type::type::INT32:
        {
            rawData = static_cast<int64>(colData->arrowInt32->Value(cellIdx));
            break;
        }
        case arrow::Type::type::INT64:
        {
            rawData = static_cast<int64>(colData->arrowInt64->Value(cellIdx));
            break;
        }
        default:
        {
            CXX_LOG_ERROR("Trying to convert non-INT type to int string: %d.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // If the Snowflake DB type is TIME, then convert to a time string.
    if (snowType == SF_DB_TYPE_TIME)
        return convertTimeToString(rawData, scale, out_data, io_len, io_capacity);

    // Otherwise, convert integral value to string and write to buffer.
    std::string strValue = std::to_string(rawData);
    allocateCharBuffer(out_data, io_len, io_capacity, strValue.size());
    *out_data = std::strncpy(*out_data, strValue.c_str(), *io_len);
    (*out_data)[*io_len] = '\0';

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DataConversions::convertTimeToString(
    int64 timeSinceMidnight,
    int64 scale,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    // Construct a tm object holding appropriate time values.
    std::tm timeObj;
    timeObj.tm_mday = 1;
    int64 secsSinceMidnight = timeSinceMidnight / std::pow(10, scale);

    std::lldiv_t divBySecs = std::lldiv(secsSinceMidnight, 60);
    std::lldiv_t divByMins = std::lldiv(divBySecs.quot, 60);
    timeObj.tm_hour = divByMins.quot;
    timeObj.tm_min = divByMins.rem;
    timeObj.tm_sec = divBySecs.rem;
    CXX_LOG_DEBUG("tm_hour: %d", timeObj.tm_hour);
    CXX_LOG_DEBUG("tm_min: %d", timeObj.tm_min);
    CXX_LOG_DEBUG("tm_sec: %d", timeObj.tm_sec);

    // Create Timestamp object. This will be used to create the end time string.
    SF_TIMESTAMP ts;
    ts.tm_obj = timeObj;
    ts.nsec = timeSinceMidnight % (int64) std::pow(10, scale);
    ts.tzoffset = 0;
    ts.scale = static_cast<int32>(scale);
    ts.ts_type = SF_DB_TYPE_TIME;

    // Leverage client library to convert to string.
    // The client will handle the allocation of resources if necessary.
    return snowflake_timestamp_to_string(&ts, "", out_data, *io_capacity, io_len, SF_BOOLEAN_TRUE);
}

SF_STATUS STDCALL DataConversions::convertTimestampToString(
    std::shared_ptr<ArrowColumn> & colData,
    SF_DB_TYPE snowType,
    int64 scale,
    uint32 colIdx,
    uint32 rowIdx,
    uint32 cellIdx,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    SF_TIMESTAMP sfts;

    // The C API has a custom struct `SF_TIMESTAMP` used to represent Timestamp values.
    // In addition to what's available in the ArrowTimestamp struct, we must provide:
    // (1) A tm object that represents the timestamp value,
    // (2) The scale of the value,
    // (3) The timestamp type, i.e. LTZ, NTZ, TZ.
    ArrowTimestampArray * ts = colData->arrowTimestamp;
    std::time_t t = ts->sse->Value(cellIdx);
    sfts.tm_obj = *(std::localtime(&t));
    sfts.nsec = (ts->fs == nullptr) ? 0 : ts->fs->Value(cellIdx);
    sfts.tzoffset = (ts->tz == nullptr) ? 0 : ts->tz->Value(cellIdx);
    sfts.scale = scale;
    sfts.ts_type = snowType;

    CXX_LOG_DEBUG("sse: %d, fs: %d, tz: %d", ts->sse->Value(cellIdx), sfts.nsec, sfts.tzoffset);

    // Leverage client library to convert to string.
    // The client will handle the allocation of resources if necessary.
    return snowflake_timestamp_to_string(&sfts, "", out_data, *io_capacity, io_len, SF_BOOLEAN_TRUE);
}

} // namespace Client
} // namespace Snowflake
