::
:: Build oob for Windows
::
@echo off
set OOB_VERSION=1.0.0
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

set CURL_VERSION=7.66.0

call "%scriptdir%\_init.bat" %platform% %build_type%
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
)
if "%build_type%"=="Release" (
    set oob_debug_option=no
        set oob_lib_name=libtelemetry_a.lib
)

if not "%VisualStudioVersion%"=="14.0" (
    echo === setting up the Visual Studio environments
    call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch%
    if %ERRORLEVEL% NEQ 0 goto :error
)

set target_name=libtelemetry_a.lib

@echo ====== Building oob
set OOB_DIR=%scriptdir%..\Source\oobTelemetry
cd %scriptdir%\..\Source\oobTelemetry
nmake -f Makefile.msc clean
nmake -f Makefile.msc CURL_DEP=%scriptdir%..\Source\curl-%CURL_VERSION% ODBC_SOURCE=%scriptdir%..\Source

set dependency=%scriptdir%..\Dependencies\%arcdir%\oob

del /q /s %dependency%
mkdir  "%dependency%\include"
mkdir  "%dependency%\lib"

copy /v /y %OOB_DIR%\*.h %dependency%\include
copy /v /y %scriptdir%\..\Source\curl-%CURL_VERSION%\lib\vtls\sf_ocsp_telemetry_data.h %dependency%\include
copy /v /y %OOB_DIR%\libtelemetry_a.lib %dependency%\lib\%target_name%

goto :success


:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
