::
:: Build oob for Windows
::
@echo off
set OOB_VERSION=1.0.4
call %*
goto :EOF

:get_version
    set version=%oob_version%
    goto :EOF

:build
setlocal
set OOB_DIR=oob-%OOB_VERSION%

set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

set scriptdir=%~dp0

set CURL_VERSION=7.78.0

call "%scriptdir%\_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

if "%platform%"=="x64" (
        set engine_dir=Program Files
)
if "%platform%"=="x86" (
        set engine_dir=Program Files (x86^)
)

if "%build_type%"=="Debug" (
    set oob_debug_option=yes
    set oob_lib_name=libtelemetry_a_debug.lib
    set COMPILEFLAG=-MTd
)
if "%build_type%"=="Release" (
    set oob_debug_option=no
    set oob_lib_name=libtelemetry_a.lib
    set COMPILEFLAG=-MT
)

set target_name=libtelemetry_a.lib

call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

@echo ====== Building oob
set OOB_SOURCE_DIR=%curdir%\deps\%OOB_DIR%

cd %OOB_SOURCE_DIR%
set CURL_DIR=curl-%CURL_VERSION%
nmake -f Makefile.msc clean
nmake -f Makefile.msc

echo === Staging oob
cd "%curdir%"
rd /q /s .\deps-build\%build_dir%\oob
md .\deps-build\%build_dir%\oob\include
md .\deps-build\%build_dir%\oob\lib
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y %OOB_SOURCE_DIR%\*.h .\deps-build\%build_dir%\oob\include
copy /v /y %OOB_SOURCE_DIR%\libtelemetry_a.lib .\deps-build\%build_dir%\oob\lib\%target_name%

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file oob %oob_version%
if %ERRORLEVEL% NEQ 0 goto :error

goto :success


:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
