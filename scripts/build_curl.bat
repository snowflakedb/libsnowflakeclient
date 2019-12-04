::
:: Build Curl
::
@echo off
set CURL_DIR=curl-7.66.0

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

if "%build_type%"=="Debug" (
    set curl_debug_option=yes
	set buildtype=debug
	set LIBDEBUG=_debug
)
if "%build_type%"=="Release" (
    set curl_debug_option=no
	set buildtype=release
	set LIBDEBUG=
)

if "%vs_version%"=="VS15" (
    set vc_version=15
)
if "%vs_version%"=="VS14" (
    set vc_version=14
)

call "%scriptdir%\utils.bat" :setup_visual_studio %vs_version%

echo === staging openssl and zlib for curl
set curl_dep=%TMP%\curl_dep
rmdir /S /Q %curl_dep%
if %ERRORLEVEL% NEQ 0 goto :error
mkdir %curl_dep%\include
if %ERRORLEVEL% NEQ 0 goto :error
mkdir %curl_dep%\include\openssl
if %ERRORLEVEL% NEQ 0 goto :error
mkdir %curl_dep%\lib
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    .\deps-build\%build_dir%\zlib\lib\zlib_a.lib ^
    %curl_dep%\lib
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y ^
    .\deps-build\%build_dir%\openssl\lib\libcrypto_a.lib ^
    %curl_dep%\lib\libcrypto.lib
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y ^
    .\deps-build\%build_dir%\openssl\lib\libssl_a.lib ^
    %curl_dep%\lib\libssl.lib
if %ERRORLEVEL% NEQ 0 goto :error
copy /v /y ^
    .\deps-build\%build_dir%\openssl\include\openssl\*.h ^
    %curl_dep%\include\openssl
copy /v /y ^
    .\deps-build\%build_dir%\zlib\include\*.h ^
    %curl_dep%\include
if %ERRORLEVEL% NEQ 0 goto :error

echo === building curl
cd "%curdir%"
set install_dir=.\deps\%CURL_DIR%\builds\libcurl-vc%vc_version%-%arch%-%buildtype%-static-ssl-static-zlib-static-ipv6-sspi
rmdir /S /Q %install_dir%-obj
rmdir /S /Q %install_dir%-obj-curl
rmdir /S /Q %install_dir%-obj-lib

cd .\deps\%CURL_DIR%\winbuild
nmake ^
    /f Makefile.vc ^
    WITH_DEVEL=%TMP%\curl_dep ^
    mode=static ^
    VC=%vc_version% ^
    WITH_SSL=static ^
    WITH_ZLIB=static ^
    ENABLE_WINSSL=no ^
	DEBUG=%curl_debug_option% ^
    MACHINE=%arch%
if %ERRORLEVEL% NEQ 0 goto :error

cd "%curdir%"
rmdir /s /q .\deps-build\%build_dir%\curl
if %ERRORLEVEL% NEQ 0 goto :error
mkdir .\deps-build\%build_dir%\curl
if %ERRORLEVEL% NEQ 0 goto :error
mkdir .\deps-build\%build_dir%\curl\bin
if %ERRORLEVEL% NEQ 0 goto :error
mkdir .\deps-build\%build_dir%\curl\lib
if %ERRORLEVEL% NEQ 0 goto :error
mkdir .\deps-build\%build_dir%\curl\include
if %ERRORLEVEL% NEQ 0 goto :error
mkdir .\deps-build\%build_dir%\curl\include\curl
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    %install_dir%\bin\curl.exe ^
    .\deps-build\%build_dir%\curl\bin
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    %install_dir%\lib\libcurl_a%LIBDEBUG%.lib ^
    .\deps-build\%build_dir%\curl\lib\libcurl_a.lib
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    %install_dir%\include\curl\*.h ^
    .\deps-build\%build_dir%\curl\include\curl
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
