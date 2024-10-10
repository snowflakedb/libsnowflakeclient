/**
 * Copyright (c) 2024 Snowflake Computing
 */

#ifndef SNOWFLAKE_CLIENTCONFIG_H
#define SNOWFLAKE_CLIENTCONFIG_H

#include "snowflake/client.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct client_config
  {
    // The log level
    char *logLevel;

    // The log path
    char *logPath;
  } client_config;

  /**
    * Construct ClientConfig from client config file passed by user. This method searches the
    * config file in following order: 1. configFilePath param which is read from connection URL or
    * connection property. 2. Environment variable: SF_CLIENT_CONFIG_FILE containing full path to
    * sf_client_config file. 3. Searches for default config file name(sf_client_config.json under the
    * driver directory from where the driver gets loaded. 4. Searches for default config file
    * name(sf_client_config.json) under user home directory 5. Searches for default config file
    * name(sf_client_config.json) under tmp directory
    * 
    * @param in_configFilePath       The config file path passed in by the user.
    * @param out_clientConfig        The ClientConfig object to be filled.
    */
  void load_client_config(
    const char* in_configFilePath,
    client_config* out_clientConfig);
 
#ifdef __cplusplus
} // extern "C"
#endif

#endif //SNOWFLAKE_CLIENTCONFIG_H
