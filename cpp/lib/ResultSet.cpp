/*
 * Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
 */

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../logger/SFLogger.hpp"
#include "snowflake/platform.h"
#include "snowflake/Simba_CRTFunctionSafe.h"
#include "ResultSet.hpp"

namespace Snowflake
{
namespace Client
{

ResultSet::ResultSet() :
    m_binaryOutputFormat("HEX"),
    m_dateOutputFormat("YYYY-MM-DD"),
    m_timeOutputFormat("HH24:MI:SS"),
    m_timestampOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampLtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampNtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3"),
    m_timestampTzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_currChunkIdx(0),
    m_currChunkRowIdx(0),
    m_currColumnIdx(0),
    m_currRowIdx(0)
{
    ;
}

ResultSet::ResultSet(SF_COLUMN_DESC * metadata, std::string tzString) :
    m_binaryOutputFormat("HEX"),
    m_dateOutputFormat("YYYY-MM-DD"),
    m_timeOutputFormat("HH24:MI:SS"),
    m_timestampOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampLtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_timestampNtzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3"),
    m_timestampTzOutputFormat("YYYY-MM-DD HH24:MI:SS.FF3 TZHTZM"),
    m_currChunkIdx(0),
    m_currChunkRowIdx(0),
    m_currColumnIdx(0),
    m_currRowIdx(0),
    m_metadata(metadata),
    m_totalChunkCount(0),
    m_totalColumnCount(0),
    m_totalRowCount(0),
    m_tzString(tzString)
{
    ;
}

// Public static methods ===========================================================================

SF_STATUS STDCALL ResultSet::convertBoolToString(
    char * value,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    const char * boolValue = (std::strcmp(value, "0") == 0) ?
        SF_BOOLEAN_FALSE_STR : SF_BOOLEAN_TRUE_STR;

    // Ensure that the buffer is well allocated.
    if (io_len == nullptr)
        io_len = new size_t(std::strlen(boolValue));

    if (*out_data == nullptr)
    {
        // If *out_data is null then io_capacity should be null as well.
        io_capacity = new size_t(*io_len);
        *out_data = new char[*io_capacity];

        CXX_LOG_DEBUG(
            "Given unallocated buffer. Allocated at 0x%x with capacity %d.",
            *out_data, *io_capacity);
    }
    else if (io_capacity != nullptr && *io_len > *io_capacity)
    {
        *io_capacity = *io_len;
        delete *out_data;
        *out_data = new char[*io_capacity];

        CXX_LOG_DEBUG(
            "Given buffer capacity less than string length. Re-allocated at 0x%x with capacity %d.",
            *out_data, *io_capacity);
    }

    *out_data = const_cast<char *>(boolValue);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSet::convertDateToString(
    char * value,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    // Probably want to move these values to a better place.
    int maxDateLength = 12;
    const char * dateFormat = "%Y-%m-%d";

    // Ensure that the buffer is well allocated.
    if (io_len == nullptr)
        io_len = new size_t(maxDateLength + 1);

    if (*out_data == nullptr)
    {
        // If *out_data is null then io_capacity should be null as well.
        io_capacity = new size_t(*io_len);
        *out_data = new char[*io_capacity];

        CXX_LOG_DEBUG(
            "Given unallocated buffer. Allocated at 0x%x with capacity %d.",
            *out_data, *io_capacity);
    }
    else if (io_capacity != nullptr && *io_len > *io_capacity)
    {
        *io_capacity = *io_len;
        delete *out_data;
        *out_data = new char[*io_capacity];

        CXX_LOG_DEBUG(
            "Given buffer capacity less than string length. Re-allocated at 0x%x with capacity %d.",
            *out_data, *io_capacity);
    }

    // The main logic for the conversion.
    struct tm tm_obj;
    struct tm *tm_ptr;
    std::memset(&tm_obj, 0, sizeof(tm_obj));
    time_t sec = (time_t) std::strtol(value, nullptr, 10) * 86400L;
    tm_ptr = sf_gmtime(&sec, &tm_obj);

    if (tm_ptr == nullptr)
    {
        if (*out_data != nullptr)
            delete *out_data;

        if (io_len != nullptr)
            delete io_len;

        if (io_capacity != nullptr)
            delete io_capacity;

        CXX_LOG_DEBUG("Failed to convert date value to string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    std::strftime(*out_data, maxDateLength + 1, dateFormat, &tm_obj);
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL ResultSet::convertTimeToString(
    char * value,
    char ** out_data,
    size_t * io_len,
    size_t * io_capacity
)
{
    SF_STATUS status;
    SF_TIMESTAMP ts;

    status = snowflake_timestamp_from_epoch_seconds(
        &ts,
        value,
        m_tzString.c_str(),
        m_metadata[m_currColumnIdx].scale,
        m_metadata[m_currColumnIdx].type);

    if (status != SF_STATUS_SUCCESS)
    {
        if (*out_data != nullptr)
            delete *out_data;

        if (io_len != nullptr)
            delete io_len;

        if (io_capacity != nullptr)
            delete io_capacity;

        CXX_LOG_DEBUG("Failed to convert time or timestamp value to string.");
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }

    return snowflake_timestamp_to_string(&ts, "", out_data, *io_capacity, io_len, SF_BOOLEAN_TRUE);
}

// Public getter methods ===========================================================================

std::string ResultSet::getBinaryOutputFormat()
{
    return m_binaryOutputFormat;
}

std::string ResultSet::getDateOutputFormat()
{
    return m_dateOutputFormat;
}

std::string ResultSet::getTimeOutputFormat()
{
    return m_timeOutputFormat;
}

std::string ResultSet::getTimestampOutputFormat()
{
    return m_timestampOutputFormat;
}

std::string ResultSet::getTimestampLtzOutputFormat()
{
    return m_timestampLtzOutputFormat;
}

std::string ResultSet::getTimestampNtzOutputFormat()
{
    return m_timestampNtzOutputFormat;
}

std::string ResultSet::getTimestampTzOutputFormat()
{
    return m_timestampTzOutputFormat;
}

QueryResultFormat ResultSet::getQueryResultFormat()
{
    return m_queryResultFormat;
}

size_t ResultSet::getTotalChunkCount()
{
    return m_totalChunkCount;
}

size_t ResultSet::getTotalColumnCount()
{
    return m_totalColumnCount;
}

size_t ResultSet::getTotalRowCount()
{
    return m_totalRowCount;
}

// Protected methods ===============================================================================

void ResultSet::initTzString()
{
    int zeroOffset = 1440;
    bool isPositiveOffset = zeroOffset >= m_tzOffset;

    char signChar = isPositiveOffset ? '+' : '-';

    // Extract HH and MM values from time offset.
    int absOffset =
        isPositiveOffset ? m_tzOffset - zeroOffset : zeroOffset - m_tzOffset;
    int hh = (int) absOffset / 60;
    int mm = absOffset % 60;

    char buffer[6];
    sb_sprintf(buffer, 6, "%c%02d:%02d", signChar, hh, mm);

    m_tzString = std::string(buffer);
}

} // namespace Client
} // namespace Snowflake
