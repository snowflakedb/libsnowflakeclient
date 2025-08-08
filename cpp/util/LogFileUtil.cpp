#include "log_file_util.h"
#include <snowflake/logger.h>
#include <boost/filesystem.hpp>

void log_warn_when_accessible_by_others(const char *filePath, const char *context, bool log_read_access)
{
#ifndef _WIN32
  try
  {
    boost::filesystem::path filePathObj(filePath);
    boost::filesystem::file_status fileStatus = boost::filesystem::status(filePathObj);
    boost::filesystem::perms permissions = fileStatus.permissions();
    log_debug("%s File %s access rights %o", context, filePath, permissions);

    if (log_read_access &&
      (permissions & boost::filesystem::group_read ||
        permissions & boost::filesystem::others_read))
    {
      log_warn("%s File %s is accessible by others with permissions %o",
        context, filePath, permissions);
    }
  }
  catch (std::exception e)
  {
    log_warn("%s, Unable to access the file to check the permission: %s. Error %s",
      context, filePath, e.what());
  }
#endif
}

void log_file_usage(const char* filePath, const char* context, bool log_read_access)
{
  log_info("%s Accessing file: %s", context, filePath);
  log_warn_when_accessible_by_others(filePath, context, log_read_access);
}
