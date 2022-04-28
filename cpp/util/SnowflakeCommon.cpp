/*
* Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
*/
#include <string.h>
#include "boost/regex.hpp"
#include "boost/filesystem.hpp"
#include "snowflake/basic_types.h"
#include "snowflake/platform.h"

/**
 * Validate partner application name.
 * No cross-platform regex lib in C so we use C++ one.
 * @param application partner application name
 */
extern "C" {

sf_bool validate_application(const char* application)
{
  boost::regex APPLICATION_REGEX = boost::regex("^[A-Za-z][A-Za-z0-9\\.\\-_]{1,50}$", boost::regex::icase);

  // It's OK to leave application unset.
  if (!application || (strlen(application) == 0))
  {
    return SF_BOOLEAN_TRUE;
  }

  if (regex_match(application, APPLICATION_REGEX))
  {
    return SF_BOOLEAN_TRUE;
  }

  return SF_BOOLEAN_FALSE;
}

int STDCALL sf_delete_directory_if_exists(const char * directoryName)
{
  if (!sf_is_directory_exist(directoryName))
  {
    return 0;
  }

  boost::system::error_code err;
  try
  {
    boost::filesystem::remove_all(boost::filesystem::path(directoryName), err);
  }
  catch (...)
  {
    return -1;
  }

  return err.value();
}

}

