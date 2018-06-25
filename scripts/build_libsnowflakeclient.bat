::
:: Build Snowflake Client library
::
@echo off
set CMAKE_PROFILE=Visual Studio 14 2015

set platform=%1
set build_type=%2
:: for ODBC, dynamic runtime needs to be set to off
set dynamic_runtime=%3
set build_tests=%4

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

if not "%VisualStudioVersion%"=="14.0" (
    echo === setting up the Visual Studio environments
    call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch%
    if %ERRORLEVEL% NEQ 0 goto :error
)

if not exist %scriptdir%\..\deps-build\%arcdir%\aws\ call "%scriptdir%\build_awssdk.bat" %platform% %build_type% %dynamic_runtime%
if %ERRORLEVEL% NEQ 0 goto :error

rmdir /q /s cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error
mkdir cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error
cd cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error

if "%arch%"=="x86" (
    cmake -G "%CMAKE_PROFILE%" -DDYNAMIC_RUNTIME=%dynamic_runtime% -DBUILD_TESTS=%build_tests% ..
) else (
    cmake -G "%CMAKE_PROFILE%" -DDYNAMIC_RUNTIME=%dynamic_runtime% -DBUILD_TESTS=%build_tests% -A %arch% ..
)
if %ERRORLEVEL% NEQ 0 goto :error
REM NOTE cmake --build doesn't work as it cannot recognize Release|Win32 profile
devenv ALL_BUILD.vcxproj /Build %build_type%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
