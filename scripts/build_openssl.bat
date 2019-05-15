::
:: Build OpenSSL
::

@echo on
set OPENSSL_DIR=openssl-1.1.1b
set CURL_DIR=curl-7.58.0

set platform=%1
set build_type=%2
set vs_version=%3

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type% %vs_version%
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
	set openssl_debug_option=--debug
)
if "%build_type%"=="Release" (
	set openssl_debug_option=
)
set crypto_target_name=libcrypto_a.lib
set ssl_target_name=libssl_a.lib
set curl_target_name=libcurl_a.lib

call "%scriptdir%\utils.bat" :setup_visual_studio %vs_version%

echo === building openssl: %curdir%\..\deps\%OPENSSL_DIR%
cd "%scriptdir%\..\deps
tar -x -z -f openssl-1.1.1b.tar.gz
cd "%scriptdir%\..\deps\%OPENSSL_DIR%"
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
rmdir /S /Q %curdir%\deps-build\%build_dir%\openssl
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%build_dir%\openssl"
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%build_dir%\openssl\bin"
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%build_dir%\openssl\include"
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%build_dir%\openssl\include\openssl"
if %ERRORLEVEL% NEQ 0 goto :error
mkdir "%curdir%\deps-build\%build_dir%\openssl\lib"
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\lib\libcrypto.lib" ^
    ".\deps-build\%build_dir%\openssl\lib\%crypto_target_name%"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\lib\libssl.lib" ^
    ".\deps-build\%build_dir%\openssl\lib\%ssl_target_name%"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\include\openssl\*.h" ^
    ".\deps-build\%build_dir%\openssl\include\openssl"

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
