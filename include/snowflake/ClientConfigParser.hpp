/**
 * Copyright (c) 2025 Snowflake Computing
 */

#ifndef SNOWFLAKE_EASYLOGGINGCONFIGPARSER_HPP
#define SNOWFLAKE_EASYLOGGINGCONFIGPARSER_HPP

#include <string>
#include "client_config_parser.h"
#include "picojson.h"

namespace Snowflake
{
namespace Client
{
  class EasyLoggingConfigParser
  {
    // Public ==================================================================
    public:
      /**
       * Constructor for client config
       */
      EasyLoggingConfigParser();

      /// @brief Destructor.
      ~EasyLoggingConfigParser();

      /**
       * Construct SFClientConfig from client config file passed by user. This method searches the
       * config file in following order: 1. configFilePath param which is read from connection URL or
       * connection property. 2. Environment variable: SF_CLIENT_CONFIG_FILE containing full path to
       * sf_client_config file. 3. Searches for default config file name(sf_client_config.json under the
       * driver directory from where the driver gets loaded. 4. Searches for default config file
       * name(sf_client_config.json) under user home directory 5. Searches for default config file
       * name(sf_client_config.json) under tmp directory
       *
       * @param configFilePath          The config file path passed in by the user.
       * @param clientConfig            The SFClientConfig object to be filled.
       * 
       * @return whether the client config was loaded properly
       */
      sf_bool loadClientConfig(
        const std::string& configFilePath,
        client_config& clientConfig);

    // Private =================================================================
    private:
      /**
       * @brief Resolve the client config path.
       *
       * @param configFilePath           The config file path passed in by the user.
       * 
       * @return The client config path
       */
      std::string resolveClientConfigPath(const std::string& configFilePath);

      /**
       * @brief Resolve home directory config path.
       *
       * @return The home directory client config path if exist, else empty.
       */
      std::string resolveHomeDirConfigPath();

      /**
       * @brief Parse JSON string.
       *
       * @param filePath                The filePath of the config file to parse.
       * @param clientConfig            The SFClientConfig object to be filled.
       * 
       * @return whether parsing the client config file was successful.
       */
      sf_bool parseConfigFile(
        const std::string& filePath,
        client_config& clientConfig);

      /**
       * @ brief Check if other have permission to modify file
       *
       * @param filePath                The file path of the config file to check permissions.
       * 
       * @return whether the file has valid permissions
       */
      sf_bool checkIfValidPermissions(const std::string& filePath);

      /**
       * @ brief Check if there are unknown entries in config file
       *
       * @param jsonString             The json object to check in json config file.
       */
      void checkUnknownEntries(picojson::value& config);

      /**
       * @ brief Get the path to the binary file
       */
      std::string getBinaryPath();
  };

} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKE_EASYLOGGINGCONFIGPARSER_HPP
