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
    size_t bufLen = 0;
    char * buf = *out_data;
    if (io_capacity)
    {
        bufLen = *io_capacity;
    }

    if (bufLen < len + 1)
    {
        if (buf)
        {
            buf = (char*)realloc(buf, len + 1);
        }
        else
        {
            buf = (char*)malloc(len + 1);
        }
        *out_data = buf;
        if (io_capacity)
        {
            *io_capacity = len + 1;
        }
    }

    if (io_len)
    {
        *io_len = len;
    }
}

} // namespace Util

namespace Conversion
{
namespace Arrow
{
// Helper methods for numeric conversion ===========================================================

SF_STATUS STDCALL IntegerToInteger(
    int64 in_data,
    int64 * out_data,
    IntegerType intType
)
{
    // Determine upper and lower limits based on buffer type.
    int64 lowerLimit = intTypeToLowerLimit[intType];
    // No need to check upper limit for unit64
    int64 upperLimit = -1;
    if (UINT64 != intType)
    {
        upperLimit = intTypeToUpperLimit[intType];
    }

    if (in_data < lowerLimit || ((upperLimit > 0) && (in_data > upperLimit)))
    {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = in_data;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL StringToInteger(
    const std::string& str_data,
    int64 * out_data,
    IntegerType intType
)
{
    // Determine upper and lower limits based on buffer type.
    int64 lowerLimit = intTypeToLowerLimit[intType];
    // No need to check upper limit for unit64
    int64 upperLimit = -1;
    if (UINT64 != intType)
    {
        upperLimit = intTypeToUpperLimit[intType];
    }

    size_t charsProcessed;
    int64 convData;
    try
    {
        convData = static_cast<int64>(std::stoll(str_data, &charsProcessed, 10));
    }
    catch (const std::out_of_range& e)
    {
        CXX_LOG_ERROR("Conversion from STRING to INTEGER failed %s.", str_data.c_str());
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    catch (...)
    {
        CXX_LOG_ERROR("Conversion from STRING to INTEGER failed %s.", str_data.c_str());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (convData < lowerLimit || ((upperLimit > 0) && (convData > upperLimit)))
    {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL StringToUint64(
    const std::string& str_data,
    uint64 * out_data
)
{
    size_t charsProcessed;
    uint64 convData;
    try
    {
        convData = static_cast<int64>(std::stoull(str_data, &charsProcessed, 10));
    }
    catch (const std::out_of_range& e)
    {
        CXX_LOG_ERROR("Conversion from STRING to UINT64 failed %s.", str_data.c_str());
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    catch (...)
    {
        CXX_LOG_ERROR("Conversion from STRING to UINT64 failed %s.", str_data.c_str());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    // All checks passed. Proceed to write to buffer.
    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL StringToDouble(
    const std::string& str_data,
    float64 * out_data
)
{
    size_t charsProcessed;
    float64 convData;
    try
    {
        convData = std::stod(str_data, &charsProcessed);
    }
    catch (const std::out_of_range& e)
    {
        CXX_LOG_ERROR("Conversion from STRING to FLOAT64 failed %s.", str_data.c_str());
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    catch (...)
    {
        CXX_LOG_ERROR("conversion from STRING to FLOAT64 failed %s.", str_data.c_str());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    if (convData == INFINITY || convData == -INFINITY)
    {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }

    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL StringToFloat(
    const std::string& str_data,
    float32 * out_data
)
{
    size_t charsProcessed;
    float32 convData;
    try
    {
        convData = static_cast<float32>(std::stof(str_data, &charsProcessed));
    }
    catch (const std::out_of_range& e)
    {
        CXX_LOG_ERROR("Conversion from STRING to FLOAT32 failed %s.", str_data.c_str());
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    catch (...)
    {
        CXX_LOG_ERROR("conversion from STRING to FLOAT32 failed %s.", str_data.c_str());
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    *out_data = convData;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL DateToString(
    int64 date,
    std::string& outString
)
{
    char dateBuf[64];
    const char * dateFormat = "%Y-%m-%d";

    // The main logic for the conversion.
    struct tm tm_obj;
    struct tm *tm_ptr;
    std::memset(&tm_obj, 0, sizeof(tm_obj));
    time_t sec = (time_t) date * 24L * 60L * 60L;
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

SF_STATUS STDCALL TimeToString(
    int64 timeSinceMidnight,
    int64 scale,
    std::string& outString
)
{
    char tsBuf[64];

    // Construct a tm object holding appropriate time values.
    std::tm timeObj;
    timeObj.tm_mday = 1;
    int64 secsSinceMidnight = timeSinceMidnight / power10[scale];

    std::lldiv_t divBySecs = std::lldiv(secsSinceMidnight, 60);
    std::lldiv_t divByMins = std::lldiv(divBySecs.quot, 60);
    timeObj.tm_hour = divByMins.quot;
    timeObj.tm_min = divByMins.rem;
    timeObj.tm_sec = divBySecs.rem;

    // Create Timestamp object. This will be used to create the end time string.
    SF_TIMESTAMP ts;
    ts.tm_obj = timeObj;
    ts.nsec = timeSinceMidnight % power10[scale];
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

} // namespace Arrow
} // namespace Conversion
} // namespace Client
} // namespace Snowflake
