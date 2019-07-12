::
:: Build Snowflake Client library
::
@echo on

set platform=%1
set build_type=%2
set vs_version=%3
:: for ODBC, dynamic runtime needs to be set to off
set dynamic_runtime=%4
set build_tests=%5

if "%platform%"=="x64" (
	set arcdir=win64
)
if "%platform%"=="x86" (
	set arcdir=win32
)

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

call "%scriptdir%\utils.bat" :setup_visual_studio %vs_version%

if not exist %scriptdir%\..\deps-build\%build_dir%\aws\lib\aws-cpp-sdk-s3.lib call "%scriptdir%\build_awssdk.bat" %platform% %build_type% %vs_version% %dynamic_runtime%
if %ERRORLEVEL% NEQ 0 goto :error

if not exist %scriptdir%\..\deps-build\%build_dir%\azure\lib\azure-storage-lite.lib call "%scriptdir%\build_azuresdk.bat" %platform% %build_type% %vs_version% %dynamic_runtime%
if %ERRORLEVEL% NEQ 0 goto :error

rmdir /q /s cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error
mkdir cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error
cd cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error

if "%arch%"=="x86" (
    cmake -G "%cmake_generator%" -DDYNAMIC_RUNTIME=%dynamic_runtime% -DBUILD_TESTS=%build_tests% -DVSDIR:STRING=%vsdir% ..
) else (
    cmake -G "%cmake_generator%" -DDYNAMIC_RUNTIME=%dynamic_runtime% -DBUILD_TESTS=%build_tests% -DVSDIR:STRING=%vsdir% -A %arch% ..
)

if %ERRORLEVEL% NEQ 0 goto :error
REM NOTE cmake --build doesn't work as it cannot recognize Release|Win32 profile
msbuild ALL_BUILD.vcxproj /property:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
