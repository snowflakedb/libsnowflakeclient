::
:: Build Azure sdk for cpp
:: GitHub repo: https://github.com/Azure/azure-sdk-for-cpp.git
::
@echo off
set azure_src_version=0.1.20
set azure_build_version=21
set azure_version=%azure_src_version%.%azure_build_version%
call %*
goto :EOF

:get_version
    set version=%azure_version%
    goto :EOF

:build
setlocal
set azure_dir=azure-sdk-for-cpp
set platform=%1
set build_type=%2
set vs_version=%3
set build_with_md=%4

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
set staticcrt=OFF
set sharedlibs=ON
if "%dynamic_runtime%"=="OFF" (
	set staticcrt=ON
	set sharedlibs=OFF
)

@echo off
set AZURE_SOURCE_DIR=%scriptdir%..\deps\%azure_dir%
set AZURE_CMAKE_BUILD_DIR=%AZURE_SOURCE_DIR%\cmake-build-%arcdir%-%vs_version%-%build_type%
set AZURE_INSTALL_DIR=%scriptdir%..\deps-build\%build_dir%\azure

rd /S /Q %AZURE_SOURCE_DIR%
git clone --single-branch --branch azure-storage-blobs_%azure_src_version% --recursive https://github.com/Azure/azure-sdk-for-cpp.git %AZURE_SOURCE_DIR%
cd %AZURE_SOURCE_DIR%
git apply ..\..\patches\azure-sdk-cpp-%azure_src_version%.patch

rd /S /Q %AZURE_CMAKE_BUILD_DIR%
md %AZURE_CMAKE_BUILD_DIR%
rd /S /Q %AZURE_INSTALL_DIR%
md %AZURE_INSTALL_DIR%
cd %AZURE_CMAKE_BUILD_DIR%

set AZURE_SDK_DISABLE_AUTO_VCPKG=1
cmake %AZURE_SOURCE_DIR% ^
-G "%cmake_generator%" -A "%cmake_architecture%" ^
-DCMAKE_BUILD_TYPE=%build_type% ^
-DCMAKE_CXX_STANDARD=17 ^
-DBUILD_SHARED_LIBS=%sharedlibs% ^
-DMSVC_USE_STATIC_CRT=%staticcrt% ^
-DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF ^
-DBUILD_TESTING=OFF ^
-DBUILD_TRANSPORT_CURL=ON ^
-DAZ_ALL_LIBRARIES=OFF ^
-DDISABLE_RUST_IN_BUILD=ON ^
-DDISABLE_AMQP=ON ^
-DDISABLE_AZURE_CORE_OPENTELEMETRY=ON ^
-DCURL_INCLUDE_DIR="%scriptdir%..\deps-build\%build_dir%\curl\include" ^
-DCURL_LIBRARY="%scriptdir%..\deps-build\%build_dir%\curl\lib\libcurl_a.lib" ^
-DOPENSSL_INCLUDE_DIR="%scriptdir%..\deps-build\%build_dir%\openssl\include" ^
-DOPENSSL_CRYPTO_LIBRARY="%scriptdir%..\deps-build\%build_dir%\openssl\lib\libcrypto_a.lib" ^
-DOPENSSL_SSL_LIBRARY="%scriptdir%..\deps-build\%build_dir%\openssl\lib\libssl_a.lib" ^
-DCMAKE_CXX_FLAGS="/D WIN32 /D _WINDOWS /EHsc /GR /W3 /Z7 /ZH:SHA_256 /guard:cf /Qspectre /sdl"

if %ERRORLEVEL% NEQ 0 goto :error

cmake --build . --target azure-storage-blobs --config %build_type%
if %ERRORLEVEL% NEQ 0 goto :error

cd "%currdir%"
xcopy /S /E /I /Y /Q  %AZURE_CMAKE_BUILD_DIR%\sdk\core\azure-core\%build_type%\azure-core.lib %AZURE_INSTALL_DIR%\lib\
xcopy /S /E /I /Y /Q  %AZURE_CMAKE_BUILD_DIR%\sdk\storage\azure-storage-common\%build_type%\azure-storage-common.lib %AZURE_INSTALL_DIR%\lib\
xcopy /S /E /I /Y /Q  %AZURE_CMAKE_BUILD_DIR%\sdk\storage\azure-storage-blobs\%build_type%\azure-storage-blobs.lib %AZURE_INSTALL_DIR%\lib\
xcopy /S /E /I /Y /Q  %AZURE_SOURCE_DIR%\sdk\core\azure-core\inc %AZURE_INSTALL_DIR%\include
xcopy /S /E /I /Y /Q  %AZURE_SOURCE_DIR%\sdk\storage\azure-storage-common\inc %AZURE_INSTALL_DIR%\include
xcopy /S /E /I /Y /Q  %AZURE_SOURCE_DIR%\sdk\storage\azure-storage-blobs\inc %AZURE_INSTALL_DIR%\include

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file azure %azure_version%
if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
exit /b 0

:error
cd "%currdir%"
exit /b 1
