::
:: Run Tests
::
@echo off
setlocal
set platform=%1
set build_type=%2
set vs_version=%3

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

call "%scriptdir%\env.bat"

if defined APPVEYOR_BUILD_ID (
    REM use the job specific schema
    set SNOWFLAKE_TEST_SCHEMA=APPVEYOR_BUILD_%APPVEYOR_BUILD_ID%
)
REM NOTE: don't combine the previous if statement and the following if statement.
REM       It is required to use the updated SNOWFLAKE_TEST_SCHEMA.
if defined APPVEYOR_BUILD_ID (
    echo === creating test schema: %SNOWFLAKE_TEST_SCHEMA%
    python .\scripts\create_schema.py
    if %ERRORLEVEL% NEQ 0 (
        echo failed to create a test schema: %SNOWFLAKE_TEST_SCHEMA%
        goto :error
    )
)

echo === running tests
cd .\cmake-build-%arcdir%
ctest -V -E "valgrind.*"
if %ERRORLEVEL% NEQ 0 (
    echo failed to run tests.
    goto :error
)
cd ..

if defined APPVEYOR_BUILD_ID (
    echo === dropping test schema: %SNOWFLAKE_TEST_SCHEMA%
    python .\scripts\drop_schema.py
    if %ERRORLEVEL% NEQ 0 (
        echo failed to drop a test schema: %SNOWFLAKE_TEST_SCHEMA%
        goto :error
    )
)

cd "%curdir%"
endlocal
exit /b 0

:error
cd "%curdir%"
endlocal
exit /b 1