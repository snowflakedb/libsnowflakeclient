::
:: Build Aws sdk 
::
@echo off

set platform=%1
set build_type=%2
set vs_version=%3
set force_shared_crt=%4

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

if "%platform%"=="x64" (	
	set engine_dir=Program Files
	set generator="%cmake_generator% Win64"
)
if "%platform%"=="x86" (
	set engine_dir=Program Files (x86^)
	set generator="%cmake_generator%"
)

set AWS_SOURCE_DIR=%scriptdir%\..\deps\aws-sdk-cpp-1.3.50\
set AWS_CMAKE_BUILD_DIR=%AWS_SOURCE_DIR%\cmake-build
set AWS_INSTALL_DIR=%scriptdir%\..\deps-build\%build_dir%\aws\

if exist %AWS_CMAKE_BUILD_DIR% rmdir /S /Q %AWS_CMAKE_BUILD_DIR%
mkdir %AWS_CMAKE_BUILD_DIR%

if not exist %AWS_INSTALL_DIR% mkdir %AWS_INSTALL_DIR%

cd %AWS_CMAKE_BUILD_DIR%

REM https://github.com/aws/aws-sdk-cpp/issues/383
set GIT_DIR=%TMP%
cmake %AWS_SOURCE_DIR% ^
-G %generator% ^
-DBUILD_ONLY=s3 ^
-DFORCE_CURL=on ^
-DCURL_LIBRARY="%scriptdir%\..\deps-build\%build_dir%\curl\lib\libcurl_a.lib" ^
-DCURL_INCLUDE_DIR="%scriptdir%\..\deps-build\%build_dir%\curl\include" ^
-DENABLE_TESTING=off ^
-DCMAKE_INSTALL_PREFIX=%AWS_INSTALL_DIR% ^
-DBUILD_SHARED_LIBS=off ^
-DSTATIC_LINKING=on ^
-DENABLE_UNITY_BUILD=on ^
-DFORCE_SHARED_CRT=%force_shared_crt%

if %ERRORLEVEL% NEQ 0 goto :error
	
msbuild INSTALL.vcxproj	/p:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
set GIT_DIR=
exit /b 0

:error
cd "%curdir%"
set GIT_DIR=
exit /b 1
