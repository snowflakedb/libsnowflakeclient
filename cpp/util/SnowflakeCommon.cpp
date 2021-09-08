/*
* Copyright (c) 2021 Snowflake Computing, Inc. All rights reserved.
*/
#include <string.h>
#include "boost/regex.hpp"
#include "snowflake/basic_types.h"

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

}

