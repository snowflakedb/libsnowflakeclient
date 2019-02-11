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

set platform=%1
set build_type=%2
set vs_version=%3

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

set target_name=
if "%build_type%"=="Debug" (
    set target_name=zlib_debug_a.lib
)
if "%build_type%"=="Release" (
    set target_name=zlib_a.lib
)

call "%scriptdir%\utils.bat" :setup_visual_studio %vs_version%

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
    cmake -G "%cmake_generator%" ..
) else (
    cmake -G "%cmake_generator%" -A %arch% ..
)
if %ERRORLEVEL% NEQ 0 goto :error
cmake --build . --config %build_type%
if %ERRORLEVEL% NEQ 0 goto :error

echo === staging zlib
cd "%curdir%"
rmdir /q /s ^
    .\deps-build\%build_dir%\zlib
mkdir .\deps-build\%build_dir%\zlib
mkdir .\deps-build\%build_dir%\zlib\include
mkdir .\deps-build\%build_dir%\zlib\lib
copy /v /y ^
    .\deps\%ZLIB_DIR%\zlib.h ^
	.\deps-build\%build_dir%\zlib\include
copy /v /y ^
    .\deps\%ZLIB_DIR%\zutil.h ^
	.\deps-build\%build_dir%\zlib\include
copy /v /y ^
    .\deps\%ZLIB_DIR%\cmake-build-%arcdir%\zconf.h ^
	.\deps-build\%build_dir%\zlib\include
copy /v /y ^
    .\deps\%ZLIB_DIR%\cmake-build-%arcdir%\%build_type%\zlibstatic.lib ^
	.\deps-build\%build_dir%\zlib\lib\%target_name%

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
