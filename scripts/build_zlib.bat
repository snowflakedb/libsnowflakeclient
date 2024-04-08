::
:: Build zlib for Windows
:: GitHub repo: https://github.com/madler/zlib.git
::
:: Prerequisite:
:: - VC 2015 or 2017
:: Arguments:
:: - x86 / x64
:: - Debug / Release
:: - vs14 / vs15
::
@echo off
set ZLIB_SRC_VERSION=1.3.1
set ZLIB_BUILD_VERSION=4
set ZLIB_VERSION=%ZLIB_SRC_VERSION%.%ZLIB_BUILD_VERSION%
call %*
goto :EOF

:get_version
    set version=%zlib_version%
    goto :EOF

:build
setlocal
set ZLIB_DIR=zlib-%ZLIB_SRC_VERSION%

set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

:: no dynamic_runtime use at the moment.
:: Somehow the same zlib_a.lib can be used for both MT and MD builds

set source_name="zlibstatic.lib"

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%
if not defined GITHUB_ACTIONS (
    if not defined WORKSPACE set WORKSPACE=%curdir%
    echo WORKSPACE=%WORKSPACE%
    set curdir=%WORKSPACE%
    cd %curdir%
)

set target_name=zlib_a.lib
if "%build_type%"=="Debug" (
    set source_name="zlibstaticd.lib"
)
if "%build_type%"=="Release" (
    set target_name=zlib_a.lib
)

echo The curdir before installing VS is %curdir%
call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%
echo The curdir after installing VS is %curdir%

echo === building zlib
echo The curdir is %curdir%

if not defined GITHUB_ACTIONS (
    echo WORKSPACE=%WORKSPACE%
    set curdir=%WORKSPACE%
    echo Changed the curdir to %curdir%
)

echo %curdir%\deps\%ZLIB_DIR%
cd %curdir%\deps\%ZLIB_DIR%
if %ERRORLEVEL% NEQ 0 goto :error

set cmake_dir=cmake-build-%arcdir%-%vs_version%-%build_type%
rd /S /Q %cmake_dir%
md %cmake_dir%
cd %cmake_dir%
if %ERRORLEVEL% NEQ 0 goto :error

cmake -G "%cmake_generator%" -A %cmake_architecture% ^
    -DCMAKE_C_FLAGS_DEBUG="/MTd /W3 /Z7 /Ob0 /Od /RTC1 /ZH:SHA_256 /guard:cf /Qspectre /sdl" ^
    -DCMAKE_C_FLAGS_RELEASE="/MT /W3 /Z7 /O2 /Ob2 /DNDEBUG /ZH:SHA_256 /guard:cf /Qspectre /sdl" ^
    ..
if %ERRORLEVEL% NEQ 0 goto :error
cmake --build . --config %build_type%
if %ERRORLEVEL% NEQ 0 goto :error

echo === staging zlib
cd "%curdir%"
rd /q /s .\deps-build\%build_dir%\zlib
md .\deps-build\%build_dir%\zlib\include
md .\deps-build\%build_dir%\zlib\lib
if %ERRORLEVEL% NEQ 0 goto :error

copy /v /y .\deps\%ZLIB_DIR%\zlib.h .\deps-build\%build_dir%\zlib\include
copy /v /y .\deps\%ZLIB_DIR%\zutil.h .\deps-build\%build_dir%\zlib\include
copy /v /y .\deps\%ZLIB_DIR%\%cmake_dir%\zconf.h .\deps-build\%build_dir%\zlib\include
copy /v /y .\deps\%ZLIB_DIR%\%cmake_dir%\%build_type%\%source_name% .\deps-build\%build_dir%\zlib\lib\%target_name%

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file zlib %zlib_version%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
