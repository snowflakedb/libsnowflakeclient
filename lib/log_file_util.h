#ifndef LOG_FILE_UTIL_H
#define LOG_FILE_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/snowflake/basic_types.h"

/**
 * Log file usage information.
 */
  void log_file_usage(const char *filePath, const char *context, sf_bool log_read_access);

#ifdef __cplusplus
}
#endif

#endif //LOG_FILE_UTIL_H
