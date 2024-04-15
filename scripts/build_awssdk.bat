::
:: Build Aws sdk
:: GitHub repo: https://github.com/aws/aws-sdk-cpp.git
::
@echo off
set aws_src_version=1.11.283
set aws_build_version=6
set aws_version=%aws_src_version%.%aws_build_version%
call %*
goto :EOF

:get_version
    set version=%aws_version%
    goto :EOF

:build
@echo off
setlocal
set aws_dir=aws-sdk-cpp-%aws_src_version%
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

set AWS_SOURCE_DIR=%scriptdir%..\deps\%aws_dir%\
set AWS_CMAKE_BUILD_DIR=%AWS_SOURCE_DIR%\cmake-build-%arcdir%-%vs_version%-%build_type%
set AWS_INSTALL_DIR=%scriptdir%..\deps-build\%build_dir%\aws\

rd /S /Q %AWS_CMAKE_BUILD_DIR%
md %AWS_CMAKE_BUILD_DIR%
rd /S /Q %AWS_INSTALL_DIR%
md %AWS_INSTALL_DIR%
cd %AWS_CMAKE_BUILD_DIR%

set CURL_LIB_PATH="%scriptdir%..\deps-build\%build_dir%\curl\lib\libcurl_a.lib"
set CURL_LIB_PATH=%CURL_LIB_PATH:\=/%
set CURL_INC_PATH="%scriptdir%..\deps-build\%build_dir%\curl\include"
set CURL_INC_PATH=%CURL_INC_PATH:\=/%
REM Keep GIT_DIR. https://github.com/aws/aws-sdk-cpp/issues/383
set GIT_DIR=%TMP%
cmake %AWS_SOURCE_DIR% ^
-G "%cmake_generator%" ^
-A "%cmake_architecture%" ^
-DBUILD_ONLY=s3 ^
-DFORCE_CURL=on ^
-DCURL_LIBRARY="%CURL_LIB_PATH%" ^
-DCURL_INCLUDE_DIR="%CURL_INC_PATH%" ^
-DENABLE_TESTING=off ^
-DAWS_STATIC_MSVC_RUNTIME_LIBRARY=ON ^
-DCMAKE_INSTALL_PREFIX=%AWS_INSTALL_DIR% ^
-DCMAKE_C_FLAGS="/D CURL_STATICLIB" ^
-DCMAKE_CXX_FLAGS="/D WIN32 /D _WINDOWS /D CURL_STATICLIB /wd4244 /EHsc /GR /ZH:SHA_256 /guard:cf /Qspectre /sdl" ^
-DBUILD_SHARED_LIBS=off ^
-DSTATIC_LINKING=on ^
-DENABLE_UNITY_BUILD=on ^
-DENABLE_CURL_LOGGING=off ^
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
