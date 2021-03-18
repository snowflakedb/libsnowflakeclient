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

#include "arrowheaders.hpp"

#include "../logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "ArrowChunkIterator.hpp"
#include "DataConversion.hpp"
#include "memory.h"
#include "ResultSet.hpp"


namespace Snowflake
{
namespace Client
{

namespace Util
{

void AllocateCharBuffer(
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity,
    size_t len
)
{
    // Write appropriate values to pointers for length and capacity containers if necessary.
    if (io_len != nullptr)
        *io_len = len;

    if (io_capacity != nullptr)
        *io_capacity = len + 1;

    // Ensure that the buffer is well-allocated.
    if (*out_data == nullptr)
        *out_data = (char *) global_hooks.calloc(1, sizeof(size_t));

    // If the provided buffer is pre-allocated but the capacity is not large enough to store
    // the string value, then re-allocate with a large enough capacity.
    if (*out_data != nullptr && *io_capacity <= len + 1)
    {
        global_hooks.dealloc(*out_data);
        *io_capacity = len + 1;
        *out_data = (char *) global_hooks.calloc(1, sizeof(char *) * (len + 1));
    }
}

} // namespace Util

namespace Conversion
{

namespace Arrow
{

// Helper methods for numeric conversion ===========================================================

SF_STATUS STDCALL NumericToSignedInteger(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
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
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL NumericToUnsignedInteger(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
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
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL NumericToDouble(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
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

SF_STATUS STDCALL NumericToFloat(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
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

SF_STATUS STDCALL StringToSignedInteger(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
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
                CXX_LOG_ERROR("Conversion from STRING to INTEGER failed.");
                CXX_LOG_ERROR("Value %s resulted in errno %d", rawData, errno);
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

SF_STATUS STDCALL StringToUnsignedInteger(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
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
                CXX_LOG_ERROR("Conversion from STRING to UNSIGNED INTEGER failed.");
                CXX_LOG_ERROR("Value %s resulted in errno %d", rawData, errno);
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

SF_STATUS STDCALL StringToDouble(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
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
                CXX_LOG_ERROR("Conversion from STRING to DOUBLE failed.");
                CXX_LOG_ERROR("Value %s resulted in errno %d", rawData, errno);
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

SF_STATUS STDCALL StringToFloat(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
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

SF_STATUS STDCALL BinaryToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
    uint32 cellIdx,
    std::string& outString
)
{
    if (arrowType->id() != arrow::Type::type::BINARY)
    {
        CXX_LOG_ERROR("Trying to convert non-BINARY type to binary string: %d.", arrowType->id());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    outString = colData->arrowBinary->GetString(cellIdx);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL BoolToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
    uint32 cellIdx,
    std::string& outString
)
{
    if (arrowType->id() != arrow::Type::type::BOOL)
    {
        CXX_LOG_ERROR("Trying to convert non-BOOL type to bool string: %d.", arrowType->id());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    bool boolValue = colData->arrowBoolean->Value(cellIdx);
    outString = (boolValue) ? SF_BOOLEAN_TRUE_STR : SF_BOOLEAN_FALSE_STR;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DateToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
    uint32 cellIdx,
    std::string& outString
)
{
    char dateBuf[64];
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
        CXX_LOG_DEBUG("Failed to convert date value to string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    std::strftime(dateBuf, sizeof(dateBuf), dateFormat, &tm_obj);
    outString = dateBuf;

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DecimalToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
    uint32 cellIdx,
    std::string& outString
)
{
    if (arrowType->id() != arrow::Type::type::DECIMAL)
    {
        CXX_LOG_ERROR("Trying to convert non-DECIMAL type to decimal string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    outString = colData->arrowDecimal128->GetString(cellIdx);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DoubleToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    size_t colIdx,
    size_t rowIdx,
    uint32 cellIdx,
    std::string& outString
)
{
    if (arrowType->id() != arrow::Type::type::DOUBLE)
    {
        CXX_LOG_ERROR("Trying to convert non-DOUBLE type to decimal string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    float64 doubleValue = colData->arrowDouble->Value(cellIdx);
    outString = std::to_string(doubleValue);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL IntToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    SF_DB_TYPE snowType,
    int64 scale,
    size_t colIdx,
    size_t rowIdx,
    uint32 cellIdx,
    std::string& outString
)
{
    // If Snowflake DB type is TIME or TIMESTAMP, then call the appropriate helper.
    if (snowType == SF_DB_TYPE_TIME)
        return TimeToString(colData, arrowType, scale, cellIdx, outString);

    if (snowType == SF_DB_TYPE_TIMESTAMP_LTZ
        || snowType == SF_DB_TYPE_TIMESTAMP_NTZ
        || snowType == SF_DB_TYPE_TIMESTAMP_TZ)
        return TimestampToString(
            colData, arrowType, snowType, scale,
            colIdx, rowIdx, cellIdx,
            outString);

    // Otherwise, convert integral value to string and write to buffer.
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

    outString = std::to_string(rawData);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL TimeToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    int64 scale,
    uint32 cellIdx,
    std::string& outString
)
{
    char tsBuf[64];
    int64 timeSinceMidnight;
    switch (arrowType->id())
    {
        case arrow::Type::type::INT32:
        {
            timeSinceMidnight = static_cast<int64>(colData->arrowInt32->Value(cellIdx));
            break;
        }
        case arrow::Type::type::INT64:
        {
            timeSinceMidnight = static_cast<int64>(colData->arrowInt64->Value(cellIdx));
            break;
        }
        default:
        {
            CXX_LOG_ERROR("Trying to convert non-INT type to time string: %d.", arrowType->id());
            return SF_STATUS_ERROR_CONVERSION_FAILURE;
        }
    }

    // Construct a tm object holding appropriate time values.
    std::tm timeObj;
    timeObj.tm_mday = 1;
    int64 secsSinceMidnight = timeSinceMidnight / std::pow(10, scale);

    std::lldiv_t divBySecs = std::lldiv(secsSinceMidnight, 60);
    std::lldiv_t divByMins = std::lldiv(divBySecs.quot, 60);
    timeObj.tm_hour = divByMins.quot;
    timeObj.tm_min = divByMins.rem;
    timeObj.tm_sec = divBySecs.rem;

    // Create Timestamp object. This will be used to create the end time string.
    SF_TIMESTAMP ts;
    ts.tm_obj = timeObj;
    ts.nsec = timeSinceMidnight % (int64) std::pow(10, scale);
    ts.tzoffset = 0;
    ts.scale = static_cast<int32>(scale);
    ts.ts_type = SF_DB_TYPE_TIME;

    // Leverage client library to convert to string.
    // The client will handle the allocation of resources if necessary.
    char * out_data = tsBuf;
    size_t len = 0;
    SF_STATUS ret = snowflake_timestamp_to_string(
                        &ts, "", &out_data, sizeof(tsBuf), &len, SF_BOOLEAN_FALSE);
    if (SF_STATUS_SUCCESS != ret)
    {
        return ret;
    }

    outString = tsBuf;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL TimestampToString(
    std::shared_ptr<ArrowColumn> & colData,
    std::shared_ptr<arrow::DataType> arrowType,
    SF_DB_TYPE snowType,
    int64 scale,
    size_t colIdx,
    size_t rowIdx,
    uint32 cellIdx,
    std::string& outString
)
{
    // The C API has a custom struct `SF_TIMESTAMP` used to represent Timestamp values.
    // In addition to what's available in the ArrowTimestamp struct, we must provide:
    // (1) A tm object that represents the timestamp value,
    // (2) The scale of the value,
    // (3) The timestamp type, i.e. LTZ, NTZ, TZ.
    char tsBuf[64];
    SF_TIMESTAMP sfts;
    sfts.scale = scale;
    sfts.ts_type = snowType;

    switch (arrowType->id())
    {
        case arrow::Type::type::INT32:
        {
            std::time_t t = colData->arrowInt32->Value(cellIdx);
            sfts.tm_obj = *(std::localtime(&t));
            sfts.nsec = 0;
            sfts.tzoffset = 0;
            break;
        }
        case arrow::Type::type::INT64:
        {
            std::time_t t = colData->arrowInt64->Value(cellIdx);
            sfts.tm_obj = *(std::localtime(&t));
            sfts.nsec = 0;
            sfts.tzoffset = 0;
            break;
        }
        case arrow::Type::type::STRUCT:
        {
            ArrowTimestampArray * ts = colData->arrowTimestamp;
            std::time_t t = ts->sse->Value(cellIdx);
            sfts.tm_obj = *(std::localtime(&t));
            sfts.nsec = (ts->fs == nullptr) ? 0 : ts->fs->Value(cellIdx);
            sfts.tzoffset = (ts->tz == nullptr) ? 0 : ts->tz->Value(cellIdx);
            break;
        }
        default:
        {
            CXX_LOG_ERROR(
                "Trying to convert non-INT or STRUCT type to timestamp string: %d.",
                arrowType->id());
        }
    }

    // Leverage client library to convert to string.
    // The client will handle the allocation of resources if necessary.
    char * out_data = tsBuf;
    size_t len = 0;
    SF_STATUS ret = snowflake_timestamp_to_string(
                        &sfts, "", &out_data, sizeof(tsBuf), &len, SF_BOOLEAN_FALSE);
    if (SF_STATUS_SUCCESS != ret)
    {
      return ret;
    }

    outString = tsBuf;
    return SF_STATUS_SUCCESS;
}

} // namespace Arrow

namespace Json
{

SF_STATUS STDCALL BoolToString(
    char * value,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    const char * boolValue = (std::strcmp(value, "0") == 0) ?
        SF_BOOLEAN_FALSE_STR : SF_BOOLEAN_TRUE_STR;
    size_t len = std::strlen(boolValue);

    // Ensure that the buffer is well allocated.
    Snowflake::Client::Util::AllocateCharBuffer(out_data, io_len, io_capacity, len);

    std::strncpy(*out_data, boolValue, len);
    (*out_data)[len] = '\0';
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DateToString(
    char * value,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    // TODO: Move these elsewhere.
    size_t maxDateLength = 12;
    const char * dateFormat = "%Y-%m-%d";

    // The main logic for the conversion.
    struct tm tm_obj;
    struct tm * tm_ptr;
    memset(&tm_obj, 0, sizeof(tm_obj));
    char * endptr;
    time_t sec = (time_t) std::strtoll(value, &endptr, 10) * 24L * 60L * 60L;
    tm_ptr = sf_gmtime(&sec, &tm_obj);

    if (tm_ptr == nullptr)
    {
        CXX_LOG_DEBUG("Failed to convert date value to string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    // Ensure that the buffer is well allocated.
    Snowflake::Client::Util::AllocateCharBuffer(out_data, io_len, io_capacity, maxDateLength);

    if (io_len != nullptr)
        *io_len = strftime(*out_data, *io_capacity, dateFormat, &tm_obj);
    else
        strftime(*out_data, *io_capacity, dateFormat, &tm_obj);

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL TimeToString(
    char * value,
    int64 scale,
    SF_DB_TYPE snowType,
    std::string tzString,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    SF_STATUS status;
    SF_TIMESTAMP ts;

    status = snowflake_timestamp_from_epoch_seconds(&ts, value, tzString.c_str(), scale, snowType);

    if (status != SF_STATUS_SUCCESS)
    {
        CXX_LOG_ERROR("Failed to convert time or timestamp value to string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    // It's okay to ignore most allocations as the client function will handle that as necessary.
    return snowflake_timestamp_to_string(&ts, "", out_data, *io_capacity, io_len, SF_BOOLEAN_TRUE);
}

} // namespace Json

} // namespace Conversion

} // namespace Client
} // namespace Snowflake
