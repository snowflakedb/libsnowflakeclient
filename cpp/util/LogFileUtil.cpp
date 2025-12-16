#include "log_file_util.h"
#include "../logger/SFLogger.hpp"
#ifndef _WIN32
#include <errno.h>
#include <sys/stat.h>
#endif

using namespace Snowflake::Client;
extern "C" {

void log_warn_when_accessible_by_others(const char *filePath, const char *context, sf_bool log_read_access)
{
#ifndef _WIN32
  struct stat ret;
  if (stat(filePath, &ret) == 0)
  {
    CXX_LOG_DEBUG("%s File %s access rights %o", context, filePath, ret.st_mode);
    if (log_read_access &&
      (ret.st_mode & S_IRGRP || ret.st_mode & S_IROTH))
    {
      CXX_LOG_WARN("%s File %s is accessible by others with permissions %o",
        context, filePath, ret.st_mode);
    }
  }
  else
  {
    CXX_LOG_WARN("%s, Unable to access the file to check the permission: %s. Error %d",
      context, filePath, errno);
  }
#endif
}

void log_file_usage(const char* filePath, const char* context, sf_bool log_read_access)
{
  CXX_LOG_INFO("%s Accessing file: %s", context, filePath);
  log_warn_when_accessible_by_others(filePath, context, log_read_access);
}

} // extern "C"
