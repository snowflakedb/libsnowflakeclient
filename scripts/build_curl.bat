::
:: Build Curl
::
:: Prerequisite:
:: - VC 2015 or 2017
:: Arguments:
:: - x86 / x64
:: - Debug / Release
:: - vs14 / vs15

@echo off
set CURL_VERSION=7.78.0.1
call %*
goto :EOF

:get_version
    set version=%CURL_VERSION%
    goto :EOF

:build
@echo off
setlocal
set CURL_DIR=curl-7.78.0


set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
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
set rtlibcfg=dynamic
if "%dynamic_runtime%"=="OFF" (
    set rtlibcfg=static
)

if "%vs_version%"=="VS16" (
    set vc_version=16
)
if "%vs_version%"=="VS15" (
    set vc_version=15
)
if "%vs_version%"=="VS14" (
    set vc_version=14
)

call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%

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
    RTLIBCFG=%rtlibcfg% ^
    DEBUG=%curl_debug_option% ^
    MACHINE=%arch%
if %ERRORLEVEL% NEQ 0 goto :error

cd "%curdir%"
rd /s /q .\deps-build\%build_dir%\curl
md .\deps-build\%build_dir%\curl\bin
md .\deps-build\%build_dir%\curl\lib
md .\deps-build\%build_dir%\curl\include\curl

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

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file curl %curl_version%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
