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

if not defined SNOWFLAKE_TEST_PRIVATE_KEY_FILE goto :after_pkey_resolve
rem Resolve relative SNOWFLAKE_TEST_PRIVATE_KEY_FILE to absolute against cwd.
rem Treat as absolute if it starts with a drive letter (X:), backslash, or slash.
set "_pkey_first=%SNOWFLAKE_TEST_PRIVATE_KEY_FILE:~0,1%"
set "_pkey_second=%SNOWFLAKE_TEST_PRIVATE_KEY_FILE:~1,1%"
if "%_pkey_second%"==":" goto :pkey_resolved
if "%_pkey_first%"=="\" goto :pkey_resolved
if "%_pkey_first%"=="/" goto :pkey_resolved
set "SNOWFLAKE_TEST_PRIVATE_KEY_FILE=%cd%\%SNOWFLAKE_TEST_PRIVATE_KEY_FILE%"
:pkey_resolved
set "_pkey_first="
set "_pkey_second="
rem Auth selection (mirrors snowflake-odbc):
rem   - If the resolved private key file exists, use keypair (JWT) auth.
rem   - Else if SNOWFLAKE_TEST_PASSWORD is set (e.g. injected by Jenkins via
rem     withCredentials), unset SNOWFLAKE_TEST_PRIVATE_KEY_FILE so that
rem     sf_test_utils.init_connection_params() falls back to password auth.
rem   - Else emit an error; the test will fail later with a clear message.
if exist "%SNOWFLAKE_TEST_PRIVATE_KEY_FILE%" goto :after_pkey_resolve
if defined SNOWFLAKE_TEST_PASSWORD (
    echo [INFO] No private key file at %SNOWFLAKE_TEST_PRIVATE_KEY_FILE%; falling back to password auth.
    set "SNOWFLAKE_TEST_PRIVATE_KEY_FILE="
) else (
    echo [ERROR] No authentication credentials found! Provide a decryptable private key ^(.p8.gpg in rsa_keys with PARAMETERS_SECRET^) or SNOWFLAKE_TEST_PASSWORD.
)
:after_pkey_resolve

echo SNOWFLAKE_TEST_ACCOUNT:           %SNOWFLAKE_TEST_ACCOUNT%
echo SNOWFLAKE_TEST_USER:              %SNOWFLAKE_TEST_USER%
echo SNOWFLAKE_TEST_DATABASE:          %SNOWFLAKE_TEST_DATABASE%
echo SNOWFLAKE_TEST_SCHEMA:            %SNOWFLAKE_TEST_SCHEMA%
echo SNOWFLAKE_TEST_ROLE:              %SNOWFLAKE_TEST_ROLE%
echo SNOWFLAKE_TEST_WAREHOUSE:         %SNOWFLAKE_TEST_WAREHOUSE%
echo SNOWFLAKE_TEST_HOST:              %SNOWFLAKE_TEST_HOST%
echo SNOWFLAKE_TEST_PORT:              %SNOWFLAKE_TEST_PORT%
echo SNOWFLAKE_TEST_PROTOCOL:          %SNOWFLAKE_TEST_PROTOCOL%
echo SNOWFLAKE_TEST_CA_BUNDLE_FILE:    %SNOWFLAKE_TEST_CA_BUNDLE_FILE%
echo SNOWFLAKE_TEST_PRIVATE_KEY_FILE:  %SNOWFLAKE_TEST_PRIVATE_KEY_FILE%
echo CLOUD_PROVIDER:                   %CLOUD_PROVIDER%
