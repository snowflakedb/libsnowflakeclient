::
:: Build OpenSSL
:: GitHub repo: https://github.com/openssl/openssl.git
::
:: Prerequisite:
:: - VC 2015 or 2017
:: Arguments:
:: - x86 / x64
:: - Debug / Release
:: - vs14 / vs15

@echo off
set OPENSSL_SRC_VERSION=3.0.13
set OPENSSL_BUILD_VERSION=3
set OPENSSL_VERSION=%OPENSSL_SRC_VERSION%.%OPENSSL_BUILD_VERSION%
call %*
goto :EOF

:get_version
    set version=%openssl_version%
    goto :EOF

:build
setlocal
set OPENSSL_DIR=openssl-%OPENSSL_SRC_VERSION%

if defined GITHUB_ACTIONS (
    set PERL_EXE=C:\Strawberry\perl\bin\perl
) else (
    set PERL_EXE=perl
)
echo === PERL_EXE: %PERL_EXE%

set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

set scriptdir=%~dp0
set "path=%scriptdir%..\ci\tools;%path%"

call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

if /I "%platform%"=="x64" (
    set openssl_target=VC-WIN64A
    set engine_dir=Program Files
)
if /I "%platform%"=="x86" (
    set openssl_target=VC-WIN32
    set engine_dir=Program Files (x86^)
)

set crypto_target_name=
set ssl_target_name=
if "%build_type%"=="Debug" (
    set openssl_debug_option=--debug
)
if "%build_type%"=="Release" (
    set openssl_debug_option=
)
set crypto_target_name=libcrypto_a.lib
set ssl_target_name=libssl_a.lib

call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%

echo === building openssl: %curdir%\..\deps\%OPENSSL_DIR%
cd "%scriptdir%..\deps\%OPENSSL_DIR%"
echo === %PERL_EXE% Configure %openssl_debug_option% %openssl_target% no-shared enable-fips /ZH:SHA_256 /Qspectre /sdl
%PERL_EXE% Configure %openssl_debug_option% %openssl_target% no-shared enable-fips /ZH:SHA_256 /Qspectre /sdl
if %ERRORLEVEL% NEQ 0 goto :error
nmake clean
if %ERRORLEVEL% NEQ 0 goto :error
nmake
if %ERRORLEVEL% NEQ 0 goto :error
rd /S /Q _install
if %ERRORLEVEL% NEQ 0 goto :error
if not exist _install md _install
if %ERRORLEVEL% NEQ 0 goto :error
REM no doc build
nmake install_sw install_ssldirs DESTDIR=.\_install
if %ERRORLEVEL% NEQ 0 goto :error
cd "%curdir%"

echo === staging openssl artifacts
rd /S /Q %curdir%\deps-build\%build_dir%\openssl
md "%curdir%\deps-build\%build_dir%\openssl\bin"
md "%curdir%\deps-build\%build_dir%\openssl\include\openssl"
md "%curdir%\deps-build\%build_dir%\openssl\lib"
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\lib\libcrypto.lib" ^
    ".\deps-build\%build_dir%\openssl\lib\%crypto_target_name%"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\providers\fips.dll" ^
    ".\deps-build\%build_dir%\openssl\lib"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\providers\fips.pdb" ^
    ".\deps-build\%build_dir%\openssl\lib"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\providers\legacy.dll" ^
    ".\deps-build\%build_dir%\openssl\lib"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\providers\legacy.pdb" ^
    ".\deps-build\%build_dir%\openssl\lib"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\bin\openssl.exe" ^
    ".\deps-build\%build_dir%\openssl\bin"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\bin\openssl.pdb" ^
    ".\deps-build\%build_dir%\openssl\bin"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\Common Files\SSL\*.cnf" ^
    ".\deps-build\%build_dir%\openssl"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\Common Files\SSL\*.dist" ^
    ".\deps-build\%build_dir%\openssl"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\lib\libssl.lib" ^
    ".\deps-build\%build_dir%\openssl\lib\%ssl_target_name%"
copy /v /y ^
    ".\deps\%OPENSSL_DIR%\_install\%engine_dir%\OpenSSL\include\openssl\*.h" ^
    ".\deps-build\%build_dir%\openssl\include\openssl"

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file openssl %openssl_version%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
