/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <cstdlib>
#include "TestSetup.hpp"

Snowflake::Client::Connection* TestSetup::connectionFactory() {
    return TestSetup::connectionWithAutocommitFactory("UTC", true);
}

Snowflake::Client::Connection* TestSetup::connectionWithAutocommitFactory(const std::string timezone,
                                                                          bool autocommit) {
    Snowflake::Client::Connection *cnx = new Snowflake::Client::Connection();

    cnx->setAttribute(SF_CON_ACCOUNT,
                      getenv("SNOWFLAKE_TEST_ACCOUNT"));
    cnx->setAttribute(SF_CON_USER,
                      getenv("SNOWFLAKE_TEST_USER"));
    cnx->setAttribute(SF_CON_PASSWORD,
                      getenv("SNOWFLAKE_TEST_PASSWORD"));
    cnx->setAttribute(SF_CON_DATABASE,
                      getenv("SNOWFLAKE_TEST_DATABASE"));
    cnx->setAttribute(SF_CON_SCHEMA,
                      getenv("SNOWFLAKE_TEST_SCHEMA"));
    cnx->setAttribute(SF_CON_ROLE,
                      getenv("SNOWFLAKE_TEST_ROLE"));
    cnx->setAttribute(SF_CON_WAREHOUSE,
                      getenv("SNOWFLAKE_TEST_WAREHOUSE"));
    cnx->setAttribute(SF_CON_AUTOCOMMIT,
                      &autocommit);
    cnx->setAttribute(SF_CON_TIMEZONE,
                      timezone.c_str());
    char *host, *port, *protocol;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host) {
        cnx->setAttribute(SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port) {
        cnx->setAttribute(SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol) {
        cnx->setAttribute(SF_CON_PROTOCOL, protocol);
    }
    return cnx;
}

std::string TestSetup::getDataDir()
{
#ifdef _WIN32
  char appveyorBuildDir[500];
  DWORD dwRet = 0;
  dwRet= GetEnvironmentVariable("APPVEYOR_BUILD_FOLDER", appveyorBuildDir, sizeof(appveyorBuildDir));
  if (dwRet != 0)
  {
    return std::string(appveyorBuildDir) + PATH_SEP + "tests" + PATH_SEP + "data" + PATH_SEP;
  }
#else
  const char * travisBuildDir = getenv("TRAVIS_BUILD_DIR");
  if (travisBuildDir)
  {
    return std::string(travisBuildDir) + PATH_SEP + "tests" + PATH_SEP + "data" + PATH_SEP;
  }
#endif

  const std::string current_file = __FILE__;
  std::string utilDir = current_file.substr(0, current_file.find_last_of(PATH_SEP));
  return utilDir + PATH_SEP + ".." + PATH_SEP + "data" + PATH_SEP;
}
