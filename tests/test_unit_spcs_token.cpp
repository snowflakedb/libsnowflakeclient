#include <string.h>
#include <string>
#include <fstream>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "connection.h"
#include "memory.h"
#include <random>
#define BOOST_ALL_NO_LIB
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include "EnvOverride.hpp"

class SpcsToken {
public:
    explicit SpcsToken(std::string token, bool generateDir = true) : m_token(std::move(token)) {
        if (generateDir) {
            GenerateRandomDir();
        }
        writeSpcsTokenFile();
    }

    ~SpcsToken() {
        removeSpcsTokenFile();
    }

    std::string getFilePath()
    {
        return m_filePath;
    }

private:
    std::string m_token;
    std::string m_filePath;

    void writeSpcsTokenFile() const {
        std::ofstream out(m_filePath, std::ios::out | std::ios::trunc);
        out << m_token;
        out.flush();
        out.close();
    }

    void removeSpcsTokenFile() const {
        (void)std::remove(m_filePath.c_str());
    }

    std::string GenerateRandomAscii()
    {
        static const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        static const size_t charset_size = sizeof(charset) - 1;

        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> distribution(0, charset_size - 1);

        std::string result;
        result.reserve(10);
        for (int i = 0; i < 10; ++i) {
            result += charset[distribution(generator)];
        }
        return result;
    }

    void GenerateRandomDir(const std::string& prefix = "spcs_token")
    {
        const std::string randomStr = GenerateRandomAscii();
        std::string tmp_cache_dir;

#if defined(_WIN32)
        const char* win_dir = getenv("WINDIR");
        tmp_cache_dir = std::string(win_dir) + "\\Temp\\" + prefix + "_" + randomStr;
        boost::filesystem::create_directories(tmp_cache_dir);
#else
        tmp_cache_dir = "/tmp/" + prefix + "_" + randomStr;
        boost::filesystem::create_directories(tmp_cache_dir);
        chmod(tmp_cache_dir.c_str(), 0700);
#endif

#ifdef _WIN32
        m_filePath = tmp_cache_dir + "\\" + "spcs_token";
#else
        m_filePath = tmp_cache_dir + "/" + "spcs_token";
#endif
    }
};


void test_json_data_not_in_spcs(void** unused)
{
    SF_UNUSED(unused);
    const std::string expectedToken = "spcs-token-value";

    char* spcs_env = sf_getenv_s(SF_SPCS_ENV_VAR, NULL, 0);
    sf_unsetenv(SF_SPCS_ENV_VAR);
    SpcsToken spcsToken(expectedToken);

    cJSON* data = snowflake_cJSON_CreateObject();
    append_spcs_token(data, spcsToken.getFilePath().c_str());

    assert_null(snowflake_cJSON_GetObjectItem(data, "SPCS_TOKEN"));

    if (spcs_env) {
        sf_setenv(SF_SPCS_ENV_VAR, spcs_env);
    }
    snowflake_cJSON_Delete(data);
}

void test_json_data_with_spcs_token_absent(void **unused)
{
    SF_UNUSED(unused);

    SpcsToken spcsToken("");

    EnvOverride envOverride(SF_SPCS_ENV_VAR, "true");
    cJSON* data = snowflake_cJSON_CreateObject();
    append_spcs_token(data, spcsToken.getFilePath().c_str());

    assert_null(snowflake_cJSON_GetObjectItem(data, "SPCS_TOKEN"));

    snowflake_cJSON_Delete(data);
}


void test_json_data_with_spcs_token_present(void **unused)
{
    SF_UNUSED(unused);

    const std::string expectedToken = "spcs-token-value";
    SpcsToken spcsToken(expectedToken);
    EnvOverride envOverride(SF_SPCS_ENV_VAR, "true");

    cJSON* data = snowflake_cJSON_CreateObject();
    append_spcs_token(data, spcsToken.getFilePath().c_str());

    cJSON* spcsTokenJson = snowflake_cJSON_GetObjectItem(data, "SPCS_TOKEN");
    assert_non_null(spcsTokenJson);
    assert_string_equal(spcsTokenJson->valuestring, expectedToken.c_str());

    snowflake_cJSON_Delete(data);
}

void test_json_data_with_spcs_token_with_white_space(void** unused)
{
    SF_UNUSED(unused);
    const std::string rawToken = "  \n spcs-token-value \n  ";
    const std::string expectedToken = "spcs-token-value";
    EnvOverride envOverride(SF_SPCS_ENV_VAR, "true");
    SpcsToken spcsToken(rawToken);

    cJSON* data = snowflake_cJSON_CreateObject();
    append_spcs_token(data, spcsToken.getFilePath().c_str());

    cJSON* spcsTokenJson = snowflake_cJSON_GetObjectItem(data, "SPCS_TOKEN");
    assert_non_null(spcsTokenJson);
    assert_string_equal(spcsTokenJson->valuestring, expectedToken.c_str());
    snowflake_cJSON_Delete(data);
}

int main(void)
{
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_json_data_not_in_spcs),
        cmocka_unit_test(test_json_data_with_spcs_token_absent),
        cmocka_unit_test(test_json_data_with_spcs_token_present),
        cmocka_unit_test(test_json_data_with_spcs_token_with_white_space),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
