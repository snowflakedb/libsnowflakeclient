::
:: Build Curl
:: GitHub repo: https://github.com/curl/curl.git

@echo off
call %*
goto :EOF

:get_version
    set version=%CURL_VERSION%
    goto :EOF

:build
@echo off
setlocal
set CURL_DIR=curl


set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set currdir=%cd%

set CURL_SRC_VERSION=%CURL_VERSION%
set CURL_BUILD_VERSION=5
set CURL_VERSION=%CURL_SRC_VERSION%.%CURL_BUILD_VERSION%

if /I "%platform%"=="x64" (    
    set openssl_target=VC-WIN64A
    set engine_dir=Program Files
)
if /I "%platform%"=="x86" (
    set openssl_target=VC-WIN32
    set engine_dir=Program Files (x86^)
)

if "%build_type%"=="Debug" (
    set curl_debug_option=yes
    set buildtype=debug
    set LIBDEBUG=-d
)
if "%build_type%"=="Release" (
    set curl_debug_option=no
    set buildtype=release
    set LIBDEBUG=
)
set rtlibcfg=dynamic
set staticcrt=OFF
set staticlibs=OFF
set sharedlibs=ON
if "%dynamic_runtime%"=="OFF" (
    set rtlibcfg=static
	set staticcrt=ON
	set staticlibs=ON
	set sharedlibs=OFF
)

if "%vs_version%"=="VS17" (
    set vc_version="Visual Studio 17 2022"
)
if "%vs_version%"=="VS16" (
    set vc_version="Visual Studio 16 2019"
)
if "%vs_version%"=="VS15" (
    set vc_version="Visual Studio 15 2017"
)

call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%

set CURL_SOURCE_DIR=%scriptdir%..\deps\%CURL_DIR%
set CURL_SRC_VERSION_GIT=%CURL_SRC_VERSION:.=_%

rd /S /Q %CURL_SOURCE_DIR%
git clone --single-branch --branch curl-%CURL_SRC_VERSION_GIT% --recursive https://github.com/curl/curl.git %CURL_SOURCE_DIR%
pushd %CURL_SOURCE_DIR%
  git apply ..\..\patches\curl-%CURL_SRC_VERSION%.patch
popd

echo === staging openssl and zlib for curl
set curl_dep=%TMP%\curl_dep
rd /S /Q %curl_dep%
md %curl_dep%\include\openssl
if %ERRORLEVEL% NEQ 0 goto :error
md %curl_dep%\lib
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y .\deps-build\%build_dir%\zlib\lib\zlib_a.lib %curl_dep%\lib
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y .\deps-build\%build_dir%\zlib\include\*.h %curl_dep%\include
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y .\deps-build\%build_dir%\openssl\lib\libcrypto_a.lib %curl_dep%\lib\libcrypto.lib
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y .\deps-build\%build_dir%\openssl\lib\libssl_a.lib %curl_dep%\lib\libssl.lib
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y .\deps-build\%build_dir%\openssl\include\openssl\*.h %curl_dep%\include\openssl
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y .\deps-build\%build_dir%\oob\lib\libtelemetry_a.lib %curl_dep%\lib\libtelemetry_a.lib
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y .\deps-build\%build_dir%\oob\include\*.h %curl_dep%\include
if %ERRORLEVEL% NEQ 0 goto :error

echo === staging cJSON for curl
set CJSON_SOURCE_DIR=%scriptdir%..\deps\cJSON-%CJSON_VERSION%\
set CJSON_PATCH=%scriptdir%..\patches\curl-cJSON-%CJSON_VERSION%.patch
rd /S /Q %CJSON_SOURCE_DIR%
git clone https://github.com/DaveGamble/cJSON.git %CJSON_SOURCE_DIR%
pushd %CJSON_SOURCE_DIR%
  git checkout tags/v%CJSON_VERSION% -b v%CJSON_VERSION%
  git apply --ignore-whitespace %CJSON_PATCH%
  copy /v /y .\cJSON.c "%currdir%\deps\%CURL_DIR%\lib\vtls\sf_cJSON.c"
  copy /v /y .\cJSON.h "%currdir%\deps\%CURL_DIR%\lib\vtls\sf_cJSON.h"
popd

echo === building curl
cd "%currdir%\deps\%CURL_DIR%"
cmake ^
. -G %vc_version% ^
-A %cmake_architecture% ^
-DCMAKE_BUILD_TYPE=%build_type% ^
-DBUILD_SHARED_LIBS=%sharedlibs% ^
-DBUILD_STATIC_LIBS=%staticlibs% ^
-DCURL_STATIC_CRT=%staticcrt% ^
-DBUILD_EXAMPLES=OFF ^
-DBUILD_LIBCURL_DOCS=OFF ^
-DBUILD_MISC_DOCS=OFF ^
-DBUILD_TESTING=OFF ^
-DOPENSSL_USE_STATIC_LIBS=ON ^
-DCURL_USE_OPENSSL=ON ^
-DOPENSSL_ROOT_DIR=%TMP%\curl_dep ^
-DOPENSSL_CRYPTO_LIBRARY=%currdir%\deps-build\%build_dir%\openss\lib\libcrypto_a.lib ^
-DCURL_ZLIB=ON ^
-DZLIB_INCLUDE_DIR=%TMP%\curl_dep\include ^
-DZLIB_LIBRARY=%currdir%\deps-build\%build_dir%\zlib\lib\zlib_a.lib ^
-DOOB_LIBRARY=%currdir%\deps-build\%build_dir%\oob\lib\libtelemetry_a.lib ^
-DCURL_USE_LIBPSL=OFF ^
-DCURL_USE_LIBSSH2=OFF ^
-DCURL_BROTLI=OFF ^
-DCURL_ZSTD=OFF ^
-DCURL_USE_LIBPSL=OFF ^
-DUSE_NGHTTP2=OFF ^
-DCURL_DISABLE_RTSP=ON ^
-DCURL_DISABLE_LDAP=ON ^
-DCURL_DISABLE_LDAPS=ON ^
-DCURL_DISABLE_TELNET=ON ^
-DCURL_DISABLE_TFTP=ON ^
-DCURL_DISABLE_IMAP=ON ^
-DCURL_DISABLE_SMB=ON ^
-DCURL_DISABLE_SMTP=ON ^
-DCURL_DISABLE_GOPHER=ON ^
-DCURL_DISABLE_POP3=ON ^
-DCURL_DISABLE_FTP=ON ^
-DCURL_DISABLE_DICT=ON ^
-DCURL_DISABLE_FILE=ON ^
-DENABLE_CURL_MANUAL=OFF
if %ERRORLEVEL% NEQ 0 goto :error

msbuild src/curl.vcxproj /p:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

cd "%currdir%"
rd /s /q .\deps-build\%build_dir%\curl
md .\deps-build\%build_dir%\curl\bin
md .\deps-build\%build_dir%\curl\lib
md .\deps-build\%build_dir%\curl\include\curl

copy /v /y ^
    .\deps\%CURL_DIR%\src\%build_type%\curl.exe ^
    .\deps-build\%build_dir%\curl\bin
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    .\deps\%CURL_DIR%\lib\%build_type%\libcurl%LIBDEBUG%.lib ^
    .\deps-build\%build_dir%\curl\lib\libcurl_a.lib
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    .\deps\%CURL_DIR%\include\curl\*.h ^
    .\deps-build\%build_dir%\curl\include\curl
if %ERRORLEVEL% NEQ 0 goto :error

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file curl %curl_version%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%currdir%"
exit /b 0

:error
cd "%currdir%"
exit /b 1
