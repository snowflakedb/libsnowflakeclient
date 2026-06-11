#ifndef SNOWFLAKE_UTIL_H
#define SNOWFLAKE_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/snowflake/basic_types.h"

/**
 * Validate str ends with the suffix
 */
sf_bool ends_with(char* str, char* suffix);

/**
 * Helper sleep function
 */
void sf_sleep_ms(int sleep_ms);

sf_bool parse_bool(const char* value, sf_bool* out);

sf_bool parse_int64(const char* value, int64* out);

sf_bool parse_int8(const char* value, int8* out);

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_UTIL_H
