::
:: Build zlib for Windows
::
:: Prerequisite:
:: - VC 2015
:: Arguments:
:: - x86 / x64
:: - Debug / Release
::
@echo off
set ZLIB_DIR=zlib-1.2.11
set CMAKE_PROFILE=Visual Studio 14 2015

set platform=%1
set build_type=%2

set scriptdir=%~dp0
call %scriptdir%\_init.bat %platform% %build_type%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

set target_name=
if "%build_type%"=="Debug" (
    set target_name=zlib_debug_a.lib
)
if "%build_type%"=="Release" (
    set target_name=zlib_a.lib
)

if not "%VisualStudioVersion%"=="14.0" (
    echo === setting up the Visual Studio environments
    call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch%
    if %ERRORLEVEL% NEQ 0 goto :error
)

echo === building zlib
cd %curdir%\deps\%ZLIB_DIR%
if %ERRORLEVEL% NEQ 0 goto :error
rmdir /S /Q cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error
mkdir cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error
cd cmake-build-%arcdir%
if %ERRORLEVEL% NEQ 0 goto :error

if "%arch%"=="x86" (
    cmake -G "%CMAKE_PROFILE%" ..
) else (
    cmake -G "%CMAKE_PROFILE%" -A %arch% ..
)
if %ERRORLEVEL% NEQ 0 goto :error
cmake --build . --config %build_type%
if %ERRORLEVEL% NEQ 0 goto :error

echo === staging zlib
cd "%curdir%"
rmdir /q /s ^
    .\deps-build\%arcdir%\zlib
mkdir .\deps-build\%arcdir%\zlib
mkdir .\deps-build\%arcdir%\zlib\include
mkdir .\deps-build\%arcdir%\zlib\lib
copy /v /y ^
    .\deps\%ZLIB_DIR%\zlib.h ^
	.\deps-build\%arcdir%\zlib\include
copy /v /y ^
    .\deps\%ZLIB_DIR%\zutil.h ^
	.\deps-build\%arcdir%\zlib\include
copy /v /y ^
    .\deps\%ZLIB_DIR%\cmake-build-%arcdir%\zconf.h ^
	.\deps-build\%arcdir%\zlib\include
copy /v /y ^
    .\deps\%ZLIB_DIR%\cmake-build-%arcdir%\%build_type%\zlibstatic.lib ^
	.\deps-build\%arcdir%\zlib\lib\%target_name%

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
