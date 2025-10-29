#include <string>
#include <fstream>
#include <chrono>
#include <array>
#include <cstdio>  
#include <memory>
#include <iostream>
#include <unistd.h>
#include <functional>

#include "wiremock.hpp"
#include "../cpp/logger/SFLogger.hpp"

namespace Snowflake {
    namespace Client {
        int WiremockRunner::S_WIREMOCK_TIMEOUT = 3;

        WiremockRunner::WiremockRunner()
        {
            CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::ctor");
            thread = std::thread(setup);
            CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::wiremock thread started");
        }

        WiremockRunner::WiremockRunner(const std::string& idpMappingFile, const std::vector<std::string>& additionalMappingFiles)
        {
            CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::ctor");
            thread = std::thread(setup);
            CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::wiremock thread started");
            CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::Configuring Idp mapping from file %s", idpMappingFile.c_str());
            waitForWiremockRunning();
            initMappingFromFile(idpMappingFile);
            for (const auto& mappingFile : additionalMappingFiles) {
                CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::Configuring additional mapping from file %s", mappingFile.c_str());
                addMappingFromFile(mappingFile);
            }
        }

        WiremockRunner::WiremockRunner(const std::string& idpMappingFile, const std::vector<std::string>& additionalMappingFiles, const int port)
        {
            CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::ctor");
            thread = std::thread(setup);
            CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::wiremock thread started");
            CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner::Configuring Idp mapping from file %s", idpMappingFile.c_str());
            waitForWiremockRunning();
            initMappingFromFile(idpMappingFile, port);
            for (const auto& mappingFile : additionalMappingFiles) {
                CXX_LOG_INFO("sf::WiremockRunner::WiremockRunner", "Configuring additional mapping from file %s", mappingFile.c_str());
                addMappingFromFile(mappingFile, port);
            }
        }

        WiremockRunner::~WiremockRunner()
        {
            CXX_LOG_INFO("sf::WiremockRunner::~WiremockRunner::dtor");
            resetMapping();
            terminateWiremock();
            thread.join();
        }

        void WiremockRunner::initMapping(const std::string& mapping)
        {
            waitForWiremockRunning();
            std::string request = std::string("curl -s")
                + " -H 'Accept: application/json'"
                + " -H 'Content-Type: application-json'"
                + " -d '" + mapping + "'"
                + " -X POST http://" + wiremockHost + ":" + wiremockAdminPort + "/__admin/mappings/import";
            int ret = std::system(request.c_str());
            CXX_LOG_INFO("sf::WiremockRunner::resetMapping::wiremock mappings initialized: %d", ret);
        }

        void WiremockRunner::initMappingFromFile(const std::string& mappingFile)
        {
            std::string mapping;
            readJSONFromFile(mappingFile, mapping);
            initMapping(mapping);
        }

        void WiremockRunner::initMappingFromFile(const std::string& mappingFile, const int port)
        {
            std::string mapping;
            readJSONFromFile(mappingFile, mapping);
            std::string portStr = std::to_string(port);
            mapping = WiremockRunner::replaceAll(mapping, "$PORT", portStr);
            initMapping(mapping);
        }

        void WiremockRunner::addMapping(const std::string& mapping)
        {
            waitForWiremockRunning();
            const std::string body = replaceAll(mapping, "\'", R"('\'')");
            const std::string request = std::string("curl -s")
                + " -H 'Accept: application/json'"
                + " -H 'Content-Type: application-json'"
                + " -d '" + body + "'"
                + " -X POST http://" + wiremockHost + ":" + wiremockAdminPort + "/__admin/mappings";
            int ret = std::system(request.c_str());
            CXX_LOG_INFO("sf::WiremockRunner::resetMapping::wiremock mappings modified: %d", ret);
        }

        void WiremockRunner::addMappingFromFile(const std::string& mappingFile)
        {
            std::string mapping;
            readJSONFromFile(mappingFile, mapping);
            addMapping(mapping);
        }

        void WiremockRunner::addMappingFromFile(const std::string& mappingFile, const int port)
        {
            std::string mapping;
            readJSONFromFile(mappingFile, mapping);
            std::string portStr = std::to_string(port);
            mapping = WiremockRunner::replaceAll(mapping, "$PORT", portStr);
            addMapping(mapping);
        }

        void WiremockRunner::resetMapping()
        {
            waitForWiremockRunning();
            std::string request = std::string("curl -s")
                + " -X POST http://" + wiremockHost + ":" + wiremockAdminPort + "/__admin/reset";
            int ret = std::system(request.c_str());
            CXX_LOG_INFO("sf::WiremockRunner::resetMapping::wiremock mappings reset: %d", ret);
        }

        std::string WiremockRunner::replaceAll(const std::string& text, const std::string& look, const std::string& replace)
        {
            std::string ret = text;
            size_t index = 0;
            while (true) {
                /* Locate the substring to replace. */
                index = ret.find(look, index);
                if (index == std::string::npos) break;

                /* Make the replacement. */
                ret.replace(index, look.length(), replace);

                /* Advance index forward so the next iteration doesn't pick it up as well. */
                index += replace.length();
            }
            return ret;
        }

        void WiremockRunner::terminateWiremock() {
            std::cout << "wiremock terminating" << std::endl;
            std::string request = std::string("curl -s")
                + " -X POST http://" + wiremockHost + ":" + wiremockAdminPort + "/__admin/shutdown";
            std::system(request.c_str());
        }

        bool WiremockRunner::isRunning() {
            const std::string request = std::string("curl -s --output /dev/null -X POST http://") + wiremockHost + ":" + wiremockAdminPort + "/__admin/mappings ";
            const int ret = std::system(request.c_str());
            CXX_LOG_INFO("sf::WiremockRunner::is Running", "%d", ret);

            return ret == 0;
        }

        void WiremockRunner::waitForWiremockRunning()
        {
            auto start = std::chrono::steady_clock::now();
            while (!isRunning())
            {
                CXX_LOG_INFO("sf::WiremockRunner::isnot running. Waiting for the execution");
                sleep(1);
                auto end = std::chrono::steady_clock::now();
                double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
                if (elapsed_ms >= WiremockRunner::S_WIREMOCK_TIMEOUT)
                {
                    CXX_LOG_INFO("sf::WiremockRunner::Wiremock startup timed out");
                    throw TimeoutException("Wiremock startup timed out!");
                }
            }
        }

        void WiremockRunner::exec(const std::string& cmd) {
            std::array<char, 128> buffer{};
            std::string result;

            auto close_file = [](FILE* f) { if (f) pclose(f); };
            std::unique_ptr<FILE, decltype(close_file)> pipe(popen(cmd.c_str(), "r"), close_file);
            if (!pipe) {
                CXX_LOG_ERROR("sf::WiremockRunner::popen() failed!");
                throw std::runtime_error("popen() failed!");
            }
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
        }

        void WiremockRunner::setup() {
            std::string path = addHomePath() + wiremockPath;
            if (access(path.c_str(), F_OK) == 0) {
                CXX_LOG_INFO("sf::WiremockRunner::setup::wiremock standalone jar found");
            }
            else {
                CXX_LOG_ERROR("sf::WiremockRunner::setup::wiremock standalone jar not found at %s", path.c_str());
                throw std::runtime_error("Wiremock standalone jar not found");
            }


            try {
                CXX_LOG_INFO("sf::WiremockRunner::setup::starting wiremock standalone");
                const std::string command = std::string("java -jar ") + addHomePath() + wiremockPath
                    + " --root-dir " + addHomePath() + wiremockHomeDir
                    + " --enable-browser-proxying"
                    + " --proxy-pass-through false"
                    + " --port " + wiremockAdminPort
                    + " --https-port " + wiremockPort
                    + " --https-keystore ../../tests/test_oauth/wiremock/ca-cert.jks"
                    + " --ca-keystore ../../tests/test_oauth/wiremock/ca-cert.jks";
                exec(command); // blocking call, will be running in a separate thread
            }
            catch (std::exception& e) {
                CXX_LOG_ERROR("sf::UnitOAuthTest::setup::Wiremock startup: %s", e.what());
            }
        }

        void WiremockRunner::readJSONFromFile(const std::string& JSONFile, std::string& res)
        {
            // read in the JSON file
            if (std::ifstream in(JSONFile, std::ios::in | std::ios::binary); in)
            {
                // seek to the end so that we can get the size for reserving space
                in.seekg(0, std::ios::end);
                res.resize(in.tellg());

                // seek back to the beginning to read the file
                in.seekg(0, std::ios::beg);
                in.read(&res[0], res.size());
                in.close();
            }
        }

        std::string WiremockRunner::addHomePath()
        {
            return std::string(std::getenv("HOME"));
        }

    }
}

