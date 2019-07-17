::
:: Build Snowflake Client dependencies
::
@echo off

set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

set scriptdir=%~dp0

call "%scriptdir%\_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

call "%scriptdir%\build_openssl.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

call "%scriptdir%\build_zlib.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

call "%scriptdir%\build_curl.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

call "%scriptdir%\build_awssdk.bat" %platform% %build_type% %vs_version% %dynamic_runtime%
if %ERRORLEVEL% NEQ 0 goto :error

call "%scriptdir%\build_azuresdk.bat" %platform% %build_type% %vs_version% %dynamic_runtime%
if %ERRORLEVEL% NEQ 0 goto :error

call "%scriptdir%\build_cmocka.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
