#pragma once
#ifndef WIREMOCK_HPP
#define WIREMOCK_HPP

#include <string>
#include <cstdlib>
#include <thread>
#include <vector>

namespace Snowflake
{
    namespace Client
    {
        using namespace std;
        std::string homeDir = std::getenv("HOME");
        const auto wiremockHomeDir = homeDir + std::string("/.wiremock");
        const auto wiremockPath = homeDir + std::string("/.m2/repository/org/wiremock/wiremock-standalone/3.8.0/wiremock-standalone-3.8.0.jar");
        constexpr auto wiremockPort = "63900";
        constexpr auto wiremockAdminPort = "8081";
        constexpr auto wiremockHost = "127.0.0.1";

        class WiremockRunner
        {
            std::thread thread;
            std::string command;

            static int S_WIREMOCK_TIMEOUT;

            static void initMapping(const std::string &mapping);
            static void addMapping(const std::string &mapping);

            static std::string replaceAll(const std::string &text, const std::string &look, const std::string &replace);

            static void terminateWiremock();
            static bool isRunning();
            static void waitForWiremockRunning();
            static void setup();
            static void exec(const std::string &cmd);
            static void readJSONFromFile(const std::string &JSONFile, std::string &res);
            static pid_t startWiremockAsync(const std::string &cmd);

        public:
            WiremockRunner();
            WiremockRunner(const std::string &idpMappingFile, const std::vector<std::string> &additionalMappingFiles);
            WiremockRunner(const std::string &idpMappingFile, const std::vector<std::string> &additionalMappingFiles, const int port);
            ~WiremockRunner();
            static void resetMapping();
            static void initMappingFromFile(const std::string &mappingFile);
            static void initMappingFromFile(const std::string &mappingFile, const int port);
            static void addMappingFromFile(const std::string &mappingFile);
            static void addMappingFromFile(const std::string &mappingFile, const int port);
        };

        class TimeoutException : public std::exception
        {
            std::string timeoutMessage;

        public:
            explicit TimeoutException(std::string message) : timeoutMessage(std::move(message)) {}
        };
    }
}

#endif
