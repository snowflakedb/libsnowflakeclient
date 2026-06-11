#include "snowflake_util.h"
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdint.h>
#include <errno.h>

sf_bool ends_with(char* str, char* suffix)
{
    size_t str_length = strlen(str);
    size_t suffix_length = strlen(suffix);
    if (suffix_length > str_length)
    {
        return SF_BOOLEAN_FALSE;
    }

    char* str_suffix = str + (str_length - suffix_length);

    return sf_strncasecmp(str_suffix, suffix, suffix_length) == 0;
}

void sf_sleep_ms(int sleep_ms)
{
#ifdef _WIN32
  Sleep(sleep_ms);
#else
  usleep(sleep_ms * 1000); // usleep takes sleep time in us (1 millionth of a second)
#endif
}

sf_bool parse_bool(const char* value, sf_bool* out)
{
    if (sf_strncasecmp(value, "true", strlen(value)) == 0 || strcmp(value, "1") == 0)
    {
        *out = SF_BOOLEAN_TRUE;
    }
    else if (sf_strncasecmp(value, "false", strlen(value)) == 0 || strcmp(value, "0") == 0)
    {
        *out = SF_BOOLEAN_FALSE;
    }
    else
    {
        return SF_BOOLEAN_FALSE;
    }
    return SF_BOOLEAN_TRUE;
}

sf_bool parse_int64(const char* value, int64* out)
{
    char* endptr;
    errno = 0;
    *out = strtoll(value, &endptr, 10);

    if (errno != 0 || endptr == value || *endptr != '\0')
    {
        return SF_BOOLEAN_FALSE;
    }
    return SF_BOOLEAN_TRUE;
}

sf_bool parse_int8(const char* value, int8* out)
{
    int64 parsed;
    if (!parse_int64(value, &parsed))
    {
        return SF_BOOLEAN_FALSE;
    }
    else if (parsed < INT8_MIN || parsed > INT8_MAX)
    {
        return SF_BOOLEAN_FALSE;
    }

    *out = (int8)parsed;
    return SF_BOOLEAN_TRUE;
}