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
 
#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_UTIL_H
