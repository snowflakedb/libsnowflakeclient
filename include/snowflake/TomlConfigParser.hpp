/**
 * Copyright (c) 2025 Snowflake Computing
 */

#ifndef SNOWFLAKE_TOMLCONFIGPARSER_HPP
#define SNOWFLAKE_TOMLCONFIGPARSER_HPP

#include <map>
#include <string>

  /**
    * Load toml configuration file.
    * 
    * @return A map of key value pairs parsed from toml file
    */
  std::map<std::string, std::string> load_toml_config();

#endif //SNOWFLAKE_TOMLCONFIGPARSER_HPP
