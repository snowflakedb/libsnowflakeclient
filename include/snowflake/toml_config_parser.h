/**
 * Copyright (c) 2025 Snowflake Computing
 */

#ifndef SNOWFLAKE_TOMLCONFIGPARSER_H
#define SNOWFLAKE_TOMLCONFIGPARSER_H

#ifdef __cplusplus
extern "C" {
#endif

  /**
    * Load toml configuration file.
    * @param connectionParams       The connection parameters parsed from the toml configuration file in k=v; format
    */
  void load_toml_config(char** connectionParams);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //SNOWFLAKE_TOMLCONFIGPARSER_H
