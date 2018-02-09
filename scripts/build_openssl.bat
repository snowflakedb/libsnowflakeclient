::
:: Build OpenSSL
::

@echo off
set OPENSSL_DIR=openssl-1.1.0g
set CURL_DIR=curl-7.58.0

set platform=%1
set build_type=%2

set scriptdir=%~dp0
call %scriptdir%\_init.bat %platform% %build_type%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

if "%platform%"=="x64" (
	set openssl_target=VC-WIN64A
	set engine_dir=Program Files
)
if "%platform%"=="x86" (
	set openssl_target=VC-WIN32
	set engine_dir=Program Files (x86^)
)

set crypto_target_name=
set ssl_target_name=
set curl_target_name=
if "%build_type%"=="Debug" (
    set crypto_target_name=libcrypto_debug_a.lib
    set ssl_target_name=libssl_debug_a.lib
    set curl_target_name=libcurl_debug_a.lib
	set openssl_debug_option=--debug
)
if "%build_type%"=="Release" (
    set crypto_target_name=libcrypto_a.lib
    set ssl_target_name=libssl_a.lib
    set curl_target_name=libcurl_a.lib
	set openssl_debug_option=
)

if not "%VisualStudioVersion%"=="14.0" (
    echo === setting up the Visual Studio environments
    call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch%
    if %ERRORLEVEL% NEQ 0 goto :error
)

echo === building openssl
cd "%curdir%\deps\%OPENSSL_DIR%"
perl Configure %openssl_debug_option% %openssl_target% no-shared
if %ERRORLEVEL% NEQ 0 goto :error
nmake clean
if %ERRORLEVEL% NEQ 0 goto :error
nmake
if %ERRORLEVEL% NEQ 0 goto :error
rmdir /S /Q _install
if %ERRORLEVEL% NEQ 0 goto :error
mkdir _install
if %ERRORLEVEL% NEQ 0 goto :error
nmake install DESTDIR=.\_install
if %ERRORLEVEL% NEQ 0 goto :error
cd "%curdir%"

echo === staging openssl artifacts
rmdir /S /Q %curdir%\deps-build\%arcdir%\openssl
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%arcdir%\openssl"
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%arcdir%\openssl\bin"
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%arcdir%\openssl\include"
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%arcdir%\openssl\include\openssl"
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%arcdir%\openssl\lib"
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\lib\libcrypto.lib" ^
    ".\deps-build\%arcdir%\openssl\lib\%crypto_target_name%"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\lib\libssl.lib" ^
    ".\deps-build\%arcdir%\openssl\lib\%ssl_target_name%"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\include\openssl\*.h" ^
    ".\deps-build\%arcdir%\openssl\include\openssl"

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
