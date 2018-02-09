::
:: Set the environment variables for tests
::
@echo off
echo === setting the test parameters
if not exist parameters.appveyor.json (
    echo "parameters.appveyor.json doesn't exist"
    exit /b 1
)

set SNOWFLAKE_TEST_CA_BUNDLE_FILE=%cd%\cacert.pem

echo @echo off>parameter.bat
jq -r ".testconnection | to_entries | map(\"set \(.key)=\(.value)\") | .[]" parameters.appveyor.json >> parameter.bat
call parameter.bat
if %ERRORLEVEL% NEQ 0 (
    echo === failed to set the test parameters
    exit /b 1
)
if not defined SNOWFLAKE_TEST_ACCOUNT (
    echo Failed to set the test parameters
	exit /b 1
)
echo Account: %SNOWFLAKE_TEST_ACCOUNT%