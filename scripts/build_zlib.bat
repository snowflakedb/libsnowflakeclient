::
:: Build zlib for Windows
:: GitHub repo: https://github.com/madler/zlib.git
::
@echo off
set ZLIB_SRC_VERSION=1.3.1
set ZLIB_BUILD_VERSION=6
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
set currdir=%cd%

set target_name=zlib_a.lib
if "%build_type%"=="Debug" (
    set source_name="zlibstaticd.lib"
)
if "%build_type%"=="Release" (
    set target_name=zlib_a.lib
)

call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%

echo === building zlib
echo %currdir%\deps\%ZLIB_DIR%
cd %currdir%\deps\%ZLIB_DIR%
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
cd "%currdir%"
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
cd "%currdir%"
exit /b 0

:error
cd "%currdir%"
exit /b 1
