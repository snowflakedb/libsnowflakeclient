::
:: Build Snowflake Client library
::
@echo off
set scriptdir=%~dp0
for /f "tokens=3" %%A in ('findstr SF_API_VERSION %scriptdir%..\include\snowflake\version.h') do @set V=%%A
set libsnowflakeclient_version=%v:"=%
call %*
goto :EOF

:get_version
	set version=%libsnowflakeclient_version%
	goto :EOF

:build
setlocal
set platform=%1
set build_type=%2
set vs_version=%3
:: for ODBC, dynamic runtime needs to be set to off
set dynamic_runtime=%4
set build_tests=%5

if not "%dynamic_runtime%"=="ON" (
	set dynamic_runtime=OFF
)
echo === dynamic_runtime: %dynamic_runtime%

if not "%build_tests%"=="ON" (
	set build_tests=OFF
)
echo === build_tests: %build_tests%

call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%

if not exist %scriptdir%..\deps-build\%build_dir%\aws\lib\aws-cpp-sdk-s3.lib call "%scriptdir%build_awssdk.bat" :build %platform% %build_type% %vs_version% %dynamic_runtime%
if %ERRORLEVEL% NEQ 0 goto :error

if not exist %scriptdir%..\deps-build\%build_dir%\azure\lib\azure-storage-lite.lib call "%scriptdir%build_azuresdk.bat" :build %platform% %build_type% %vs_version% %dynamic_runtime%
if %ERRORLEVEL% NEQ 0 goto :error

set cmake_dir=cmake-build-%arcdir%-%vs_version%-%build_type%
rd /q /s %cmake_dir%
md %cmake_dir%
cd %cmake_dir%
if %ERRORLEVEL% NEQ 0 goto :error

:: Simlink does not work on Appveyor (mlink doesn't work either).
if exist  %curdir%\tests\test_simple_put_azure.cpp del %curdir%\tests\test_simple_put_azure.cpp
copy %curdir%\tests\test_simple_put.cpp %curdir%\tests\test_simple_put_azure.cpp

if "%arch%"=="x86" (
    cmake -G "%cmake_generator%" ^
	    -DDYNAMIC_RUNTIME=%dynamic_runtime% ^
		-DBUILD_TESTS=%build_tests% ^
		-DCMAKE_BUILD_TYPE=%build_type% ^
		-DVSDIR:STRING=%vsdir% ..
) else (
    cmake -G "%cmake_generator%" ^
		-DDYNAMIC_RUNTIME=%dynamic_runtime% ^
		-DBUILD_TESTS=%build_tests% ^
		-DCMAKE_BUILD_TYPE=%build_type% ^
		-DVSDIR:STRING=%vsdir% -A %arch% ..
)

if %ERRORLEVEL% NEQ 0 goto :error
:: cmake --build doesn't work as it cannot recognize Release|Win32 profile
msbuild ALL_BUILD.vcxproj /property:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error


cd "%curdir%"
rmdir /s /q .\deps-build\%build_dir%\libsnowflakeclient
if %ERRORLEVEL% NEQ 0 goto :error

md .\deps-build\%build_dir%\libsnowflakeclient\lib
if %ERRORLEVEL% NEQ 0 goto :error
md .\deps-build\%build_dir%\libsnowflakeclient\include\snowflake
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    .\%cmake_dir%\%build_type%\snowflakeclient.* ^
	.\deps-build\%build_dir%\libsnowflakeclient\lib

copy /v /y ^
    .\include\snowflake\* ^
	.\deps-build\%build_dir%\libsnowflakeclient\include\snowflake\*

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file libsnowflakeclient %libsnowflakeclient_version%
if %ERRORLEVEL% NEQ 0 goto :error
if not "%build_tests%"=="OFF" (
    call :get_version
    call "%scriptdir%utils.bat" :get_zip_file_name libsnowflakeclient %version%
    7z a artifacts\%zip_cmake_file_name% %cmake_dir%
    if %ERRORLEVEL% NEQ 0 goto :error
    7z l artifacts\%zip_cmake_file_name%
    if %ERRORLEVEL% NEQ 0 goto :error
)
:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
