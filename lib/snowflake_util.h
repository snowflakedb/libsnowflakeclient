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

/*
* Parse boolean value from string. Acceptable true values are "true" (case insensitive) and "1". Acceptable false values are "false" (case insensitive) and "0".
* 
* @return SF_BOOLEAN_TRUE if the value was successfully parsed as a boolean, otherwise SF_BOOLEAN_FALSE.
*/
sf_bool parse_bool(const char* value, sf_bool* out);

/*
* Parse int64 value from string.
* 
* @return SF_BOOLEAN_TRUE if the value was successfully parsed as an int64, otherwise SF_BOOLEAN_FALSE.
*/
sf_bool parse_int64(const char* value, int64* out);

/*
* Parse int8 value from string.
* 
* @return SF_BOOLEAN_TRUE if the value was successfully parsed as an int8, otherwise SF_BOOLEAN_FALSE.
*/
sf_bool parse_int8(const char* value, int8* out);

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_UTIL_H
