#include <cassert>
#include <string>
#include "memory.h"
#include <vector>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

typedef struct sf_db_types
{
    SF_DB_TYPE sf_dbType;
    std::string value;
} sf_db_types;

std::vector<sf_db_types> sfDbTypes = {
    {SF_DB_TYPE_FIXED, "FIXED"},
    {SF_DB_TYPE_REAL, "REAL"},
    {SF_DB_TYPE_TEXT, "TEXT"},
    {SF_DB_TYPE_DATE, "DATE"},
    {SF_DB_TYPE_TIMESTAMP_LTZ, "TIMESTAMP_LTZ"},
    {SF_DB_TYPE_TIMESTAMP_NTZ, "TIMESTAMP_NTZ"},
    {SF_DB_TYPE_TIMESTAMP_TZ, "TIMESTAMP_TZ"},
    {SF_DB_TYPE_VARIANT, "VARIANT"},
    {SF_DB_TYPE_OBJECT, "OBJECT"},
    {SF_DB_TYPE_ARRAY, "ARRAY"},
    {SF_DB_TYPE_BINARY, "BINARY"},
    {SF_DB_TYPE_TIME, "TIME"},
    {SF_DB_TYPE_BOOLEAN, "BOOLEAN"},
    {SF_DB_TYPE_ANY, "ANY"},
};

typedef struct sf_c_types
{
    SF_C_TYPE sf_cType;
    std::string value;
} sf_c_types;

std::vector<sf_c_types> sfCTypes = {
    {SF_C_TYPE_INT8, "SF_C_TYPE_INT8"},
    {SF_C_TYPE_UINT8, "SF_C_TYPE_UINT8"},
    {SF_C_TYPE_INT64, "SF_C_TYPE_INT64"},
    {SF_C_TYPE_UINT64, "SF_C_TYPE_UINT64"},
    {SF_C_TYPE_FLOAT64, "SF_C_TYPE_FLOAT64"},
    {SF_C_TYPE_STRING, "SF_C_TYPE_STRING"},
    {SF_C_TYPE_TIMESTAMP, "SF_C_TYPE_TIMESTAMP"},
    {SF_C_TYPE_BOOLEAN, "SF_C_TYPE_BOOLEAN"},
    {SF_C_TYPE_BINARY, "SF_C_TYPE_BINARY"},
    {SF_C_TYPE_NULL, "SF_C_TYPE_NULL"},
};

// unit test for snowflake_type_to_string() for all SF_DB_TYPE
void test_snowflake_type_to_string(void **unused)
{
    std::string value;
    for (sf_db_types type : sfDbTypes)
    {
        value = snowflake_type_to_string(type.sf_dbType);
        assert_string_equal(value.c_str(), type.value.c_str());
        value.clear();
    }
}

// unit test test_snowflake_c_type_to_string() for all SF_C_TYPE
void test_snowflake_c_type_to_string(void **unused)
{
    std::string value;
    for (sf_c_types type : sfCTypes)
    {
        value = snowflake_c_type_to_string(type.sf_cType);
        assert_string_equal(value.c_str(), type.value.c_str());
        value.clear();
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_snowflake_type_to_string),
        cmocka_unit_test(test_snowflake_c_type_to_string)};
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    return ret;
}
