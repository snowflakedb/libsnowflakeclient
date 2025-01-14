#include "util.h"
#include <string.h>

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