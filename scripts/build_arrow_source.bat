::
:: Build arrow
:: GitHub repo: https://github.com/apache/arrow.git
::
@echo off
call %*
goto :EOF

:get_version
    set version=%arrow_version%
    goto :EOF

:build
setlocal
set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

set currdir=%cd%

if /I "%platform%"=="x64" (
    set engine_dir=Program Files
)
if /I "%platform%"=="x86" (
    set engine_dir=Program Files (x86^)
)

set ARROW_SOURCE_DIR=%scriptdir%..\deps\arrow-%arrow_src_version%
set ARROW_CMAKE_BUILD_DIR=%ARROW_SOURCE_DIR%\cpp\cmake-build-%arcdir%-%vs_version%-%build_type%
set DEPENDENCY_DIR=%scriptdir%..\deps-build\%build_dir%
set ARROW_INSTALL_DIR=%DEPENDENCY_DIR%\arrow
set ARROW_DEPS_INSTALL_DIR=%DEPENDENCY_DIR%\arrow_deps

rd /S /Q %ARROW_CMAKE_BUILD_DIR%
md %ARROW_CMAKE_BUILD_DIR%
rd /S /Q %ARROW_INSTALL_DIR%
md %ARROW_INSTALL_DIR%
rd /S /Q %ARROW_DEPS_INSTALL_DIR%
md %ARROW_DEPS_INSTALL_DIR%
cd %ARROW_CMAKE_BUILD_DIR%

set runtimelink=MultiThreaded
if /I "%build_type%"=="debug" (
    set debugging=on
	set variant=debug
	set runtimelink=%runtimelink%Debug
) else (
    set debugging=off
	set variant=release
)

if /I "%dynamic_runtime%"=="on" (
	set runtimelink=%runtimelink%DLL
)

cmake ..\ ^
-G "%cmake_generator%" -A "%cmake_architecture%" ^
-DCMAKE_BUILD_TYPE=%build_type% ^
-DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF ^
-DCMAKE_INSTALL_PREFIX=%ARROW_INSTALL_DIR% ^
-DCMAKE_MSVC_RUNTIME_LIBRARY=%runtimelink% ^
-DCMAKE_C_FLAGS="/guard:cf /Z7 /ZH:SHA_256 /Qspectre /sdl" ^
-DCMAKE_CXX_FLAGS="/std:c++17 /guard:cf /Z7 /ZH:SHA_256 /Qspectre /sdl" ^
-DARROW_USE_STATIC_CRT=ON ^
-DARROW_BOOST_USE_SHARED=OFF ^
-DARROW_BUILD_SHARED=OFF ^
-DARROW_BUILD_STATIC=ON ^
-DBOOST_SOURCE=SYSTEM ^
-DARROW_WITH_BROTLI=OFF ^
-DARROW_WITH_LZ4=OFF ^
-DARROW_WITH_SNAPPY=OFF ^
-DARROW_WITH_ZLIB=OFF ^
-DARROW_JSON=OFF ^
-DARROW_DATASET=OFF ^
-DARROW_BUILD_UTILITIES=OFF ^
-DARROW_COMPUTE=OFF ^
-DARROW_FILESYSTEM=OFF ^
-DARROW_USE_GLOG=OFF ^
-DARROW_HDFS=OFF ^
-DARROW_WITH_BACKTRACE=OFF ^
-DARROW_JEMALLOC_USE_SHARED=OFF ^
-DARROW_BUILD_TESTS=OFF ^
-DBoost_INCLUDE_DIR=%DEPENDENCY_DIR%\boost\include ^
-DBOOST_SYSTEM_LIBRARY=%DEPENDENCY_DIR%\boost\lib\libboost_system.lib ^
-DBOOST_FILESYSTEM_LIBRARY=%DEPENDENCY_DIR%\boost\lib\libboost_filesystem.lib

if %ERRORLEVEL% NEQ 0 goto :error

msbuild INSTALL.vcxproj /p:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

cd "%currdir%"

goto :success

:success
exit /b 0

:error
cd "%currdir%"
exit /b 1
