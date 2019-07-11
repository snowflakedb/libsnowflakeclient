::
:: Build Azure cpp storage light sdk
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
set build_dir=""

if "%platform%"=="x64" (
	set engine_dir=Program Files
	set generator="%cmake_generator% Win64"
	set build_dir=win64
)
if "%platform%"=="x86" (
	set engine_dir=Program Files (x86^)
	set generator="%cmake_generator%"
	set build_dir=win32
)

set AZURE_SOURCE_DIR=%scriptdir%\..\deps\azure-storage-cpplite
set AZURE_CMAKE_BUILD_DIR=%AZURE_SOURCE_DIR%\cmake-build
set AZURE_INSTALL_DIR=%scriptdir%\..\deps-build\%build_dir%\%vs_version%\azure
set GIT_REPO="https://github.com/snowflakedb/azure-storage-cpplite.git"
set CLONE_CMD="git clone -b master $GIT_REPO $AZURE_SOURCE_DIR"
set VERSION="v0.1.8"
set GIT=git.exe

%GIT% clone %GIT_REPO% %AZURE_SOURCE_DIR%

if exist %AZURE_CMAKE_BUILD_DIR% rmdir /S /Q %AZURE_CMAKE_BUILD_DIR%
mkdir %AZURE_CMAKE_BUILD_DIR%

if not exist %AZURE_INSTALL_DIR% mkdir %AZURE_INSTALL_DIR%

cd %AZURE_CMAKE_BUILD_DIR%

if exist %AZURE_SOURCE_DIR% (
	%GIT% fetch
	%GIT% checkout tags/%VERSION% -b %VERSION%
	)

cmake %AZURE_SOURCE_DIR% ^
-DCMAKE_BUILD_TYPE=%build_type% ^
-DCMAKE_GENERATOR_PLATFORM=%platform% ^
-DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF ^
-DFORCE_SHARED_CRT=%force_shared_crt% ^
-DBUILD_SHARED_LIBS=OFF ^
-DUSE_OPENSSL=true ^
-DCURL_LINK_TYPE=static ^
-DOPENSSL_LINK_TYPE=static ^
-DOPENSSL_VERSION_NUMBER=0x11100000L ^
-DOPENSSL_INCLUDE_DIR="%scriptdir%\..\deps-build\%build_dir%\%vs_version%\openssl\include" ^
-DOPENSSL_CRYPTO_LIBRARY="%scriptdir%\..\deps-build\%build_dir%\%vs_version%\openssl\lib\libcrypto_a.lib" ^
-DOPENSSL_SSL_LIBRARY="%scriptdir%\..\deps-build\%build_dir%\%vs_version%\openssl\lib\libssl_a.lib" ^
-DCURL_INCLUDE_DIR="%scriptdir%\..\deps-build\%build_dir%\%vs_version%\curl\include" ^
-DCURL_LIBRARY="%scriptdir%\..\deps-build\%build_dir%\%vs_version%\curl\lib\libcurl_a.lib" ^
-DBUILD_TESTS=false ^
-DBUILD_SAMPLES=false

if %ERRORLEVEL% NEQ 0 goto :error

msbuild INSTALL.vcxproj	/p:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
xcopy /S /E /I /Y /Q  %AZURE_CMAKE_BUILD_DIR%\%build_type%\azure-storage-lite.lib %AZURE_INSTALL_DIR%\lib\
xcopy /S /E /I /Y /Q  %AZURE_SOURCE_DIR%\include %AZURE_INSTALL_DIR%\include
exit /b 0

:error
cd "%curdir%"
exit /b 1
