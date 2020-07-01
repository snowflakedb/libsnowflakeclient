::
:: Set the environment variables for tests
::
@echo off
echo === setting the test parameters
if not exist parameters.json (
    echo parameters.json doesn't exist
    exit /b 1
)

set SNOWFLAKE_TEST_CA_BUNDLE_FILE=%cd%\cacert.pem

echo @echo off>parameters.bat
jq -r ".testconnection | to_entries | map(\"set \(.key)=\(.value)\") | .[]" parameters.json >> parameters.bat
call parameters.bat
if %ERRORLEVEL% NEQ 0 (
    echo === failed to set the test parameters
    exit /b 1
)
if not defined SNOWFLAKE_TEST_ACCOUNT (
    echo Failed to set the test parameters
    exit /b 1
)
echo SNOWFLAKE_TEST_ACCOUNT:        %SNOWFLAKE_TEST_ACCOUNT%
echo SNOWFLAKE_TEST_USER:           %SNOWFLAKE_TEST_USER%
echo SNOWFLAKE_TEST_DATABASE:       %SNOWFLAKE_TEST_DATABASE%
echo SNOWFLAKE_TEST_SCHEMA:         %SNOWFLAKE_TEST_SCHEMA%
echo SNOWFLAKE_TEST_ROLE:           %SNOWFLAKE_TEST_ROLE%
echo SNOWFLAKE_TEST_WAREHOUSE:      %SNOWFLAKE_TEST_WAREHOUSE%
echo SNOWFLAKE_TEST_HOST:           %SNOWFLAKE_TEST_HOST%
echo SNOWFLAKE_TEST_PORT:           %SNOWFLAKE_TEST_PORT%
echo SNOWFLAKE_TEST_PROTOCOL:       %SNOWFLAKE_TEST_PROTOCOL%
echo SNOWFLAKE_TEST_CA_BUNDLE_FILE: %SNOWFLAKE_TEST_CA_BUNDLE_FILE%
echo CLOUD_PROVIDER: 		     %CLOUD_PROVIDER%
