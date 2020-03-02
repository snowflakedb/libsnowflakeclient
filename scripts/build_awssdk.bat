::
:: Build Aws sdk 
::
@echo off
set aws_version=1.3.50
call %*
goto :EOF

:get_version
    set version=%aws_version%
    goto :EOF

:build
@echo off
setlocal
set platform=%1
set build_type=%2
set vs_version=%3
set force_shared_crt=%4

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

set curdir=%cd%

if /I "%platform%"=="x64" (    
    set engine_dir=Program Files
)
if /I "%platform%"=="x86" (
    set engine_dir=Program Files (x86^)
)

set AWS_SOURCE_DIR=%scriptdir%..\deps\aws-sdk-cpp-%aws_version%\
set AWS_CMAKE_BUILD_DIR=%AWS_SOURCE_DIR%\cmake-build-%arcdir%-%vs_version%-%build_type%
set AWS_INSTALL_DIR=%scriptdir%..\deps-build\%build_dir%\aws\

rd /S /Q %AWS_CMAKE_BUILD_DIR%
md %AWS_CMAKE_BUILD_DIR%
rd /S /Q %AWS_INSTALL_DIR%
md %AWS_INSTALL_DIR%
cd %AWS_CMAKE_BUILD_DIR%

REM Keep GIT_DIR. https://github.com/aws/aws-sdk-cpp/issues/383
set GIT_DIR=%TMP%
cmake %AWS_SOURCE_DIR% ^
-G "%cmake_generator%" ^
-A "%cmake_architecture%" ^
-DBUILD_ONLY=s3 ^
-DFORCE_CURL=on ^
-DCURL_LIBRARY="%scriptdir%..\deps-build\%build_dir%\curl\lib\libcurl_a.lib" ^
-DCURL_INCLUDE_DIR="%scriptdir%..\deps-build\%build_dir%\curl\include" ^
-DENABLE_TESTING=off ^
-DCMAKE_INSTALL_PREFIX=%AWS_INSTALL_DIR% ^
-DBUILD_SHARED_LIBS=off ^
-DSTATIC_LINKING=on ^
-DENABLE_UNITY_BUILD=on ^
-DFORCE_SHARED_CRT=%force_shared_crt%

if %ERRORLEVEL% NEQ 0 goto :error
    
msbuild INSTALL.vcxproj /p:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

cd "%curdir%"

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file aws %aws_version%
if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
