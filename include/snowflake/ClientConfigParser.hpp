/**
 * Copyright (c) 2024 Snowflake Computing
 */

#ifndef SNOWFLAKE_CONFIGPARSER_HPP
#define SNOWFLAKE_CONFIGPARSER_HPP

#include <string>

namespace Snowflake
{
namespace Client
{
  struct ClientConfig
  {
    // The log level
    std::string logLevel = "";

    // The log path
    std::string logPath = "";
  };

  struct ClientConfigException : public std::exception
  {
    ClientConfigException(const std::string& message) : message_(message) {}
    const char* what() const noexcept
    {
      return message_.c_str();
    }

    std::string message_;
  };

  class ClientConfigParser
  {
    // Public ==================================================================
    public:
      /**
       * Constructor for client config
       */
      ClientConfigParser();

      /// @brief Destructor.
      ~ClientConfigParser();

      /**
       * Construct SFClientConfig from client config file passed by user. This method searches the
       * config file in following order: 1. configFilePath param which is read from connection URL or
       * connection property. 2. Environment variable: SF_CLIENT_CONFIG_FILE containing full path to
       * sf_client_config file. 3. Searches for default config file name(sf_client_config.json under the
       * driver directory from where the driver gets loaded. 4. Searches for default config file
       * name(sf_client_config.json) under user home directory 5. Searches for default config file
       * name(sf_client_config.json) under tmp directory
       *
       * @param in_configFilePath       The config file path passed in by the user.
       * @param out_clientConfig        The SFClientConfig object to be filled.
       */
      void loadClientConfig(
        const std::string& in_configFilePath,
        ClientConfig& out_clientConfig);

    // Private =================================================================
    private:
      /**
       * @brief Check if the file exists.
       *
       * @param in_filePath             The file path to check.
       */
      bool checkFileExists(const std::string& in_filePath);

      /**
       * @brief Parse JSON string.
       *
       * @param in_filePath             The filePath of the config file to parse.
       * @param out_clientConfig        The SFClientConfig object to be filled.
       */
      void parseConfigFile(
        const std::string& in_filePath,
        ClientConfig& out_clientConfig);

      /**
       * @ brief Check if other have permission to modify file
       *
       * @param in_filePath             The file path of the config file to check permissions.
       */
      void checkIfValidPermissions(const std::string& in_filePath);

      /**
       * @ brief Check if there are unknown entries in config file
       *
       * @param in_jsonString           The json object to check in json config file.
       */
      void checkUnknownEntries(const std::string& in_jsonString);

      /**
       * @ brief Get the path to the binary file
       */
      std::string getBinaryPath();
  };

} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKE_CONFIGPARSER_HPP
