::
:: Run Tests
::
@echo off
set platform=%1
set build_type=%2

set scriptdir=%~dp0
call %scriptdir%\_init.bat %platform% %build_type%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

call %scriptdir%\env.bat

if defined APPVEYOR_BUILD_ID (
	:: use the job specific schema
    set SNOWFLAKE_TEST_SCHEMA=APPVEYOR_BUILD_%APPVEYOR_BUILD_ID%
)
:: NOTE: don't combine the previous if statement and the following if statement.
::       It is required to use the updated SNOWFLAKE_TEST_SCHEMA.
if defined APPVEYOR_BUILD_ID (
    echo === creating test schema: %SNOWFLAKE_TEST_SCHEMA%
    python .\scripts\create_schema.py
    if %ERRORLEVEL% NEQ 0 (
	    echo failed to create a test schema: %SNOWFLAKE_TEST_SCHEMA%
	    goto :error
	)
)

echo === running tests
for /r ".\cmake-build-%arcdir%\examples\Release" %%a in (*.exe) do (
    echo === %%~fa
    %%~fa
    ::if %ERRORLEVEL% NEQ 0 (
    ::    echo Failed to run test: %%~fa
    ::    goto :error
    ::)
)

if defined APPVEYOR_BUILD_ID (
    echo === dropping test schema: %SNOWFLAKE_TEST_SCHEMA%
    python .\scripts\drop_schema.py
    if %ERRORLEVEL% NEQ 0 (
	    echo failed to drop a test schema: %SNOWFLAKE_TEST_SCHEMA%
	    goto :error
	)
)

cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1