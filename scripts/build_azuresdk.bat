::
:: Build Azure cpp storage light sdk
:: GitHub repo: https://github.com/snowflakedb/azure-storage-cpplite.git
::
@echo off
set azure_src_version=0.1.20
set azure_build_version=12
set azure_version=%azure_src_version%.%azure_build_version%
call %*
goto :EOF

:get_version
    set version=%azure_version%
    goto :EOF

:build
setlocal
set azure_dir=azure-storage-cpplite-%azure_src_version%
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

@echo off
set AZURE_SOURCE_DIR=%scriptdir%..\deps\%azure_dir%
set AZURE_CMAKE_BUILD_DIR=%AZURE_SOURCE_DIR%\cmake-build-%arcdir%-%vs_version%-%build_type%
set AZURE_INSTALL_DIR=%scriptdir%..\deps-build\%build_dir%\azure

rd /S /Q %AZURE_CMAKE_BUILD_DIR%
md %AZURE_CMAKE_BUILD_DIR%
rd /S /Q %AZURE_INSTALL_DIR%
md %AZURE_INSTALL_DIR%
cd %AZURE_CMAKE_BUILD_DIR%

cmake %AZURE_SOURCE_DIR% ^
-G "%cmake_generator%" -A "%cmake_architecture%" ^
-DCMAKE_BUILD_TYPE=%build_type% ^
-DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF ^
-DBUILD_WITH_MD=%build_with_md% ^
-DBUILD_SHARED_LIBS=OFF ^
-DUSE_OPENSSL=true ^
-DCURL_LINK_TYPE=static ^
-DOPENSSL_LINK_TYPE=static ^
-DOPENSSL_VERSION_NUMBER=0x11100000L ^
-DOPENSSL_INCLUDE_DIR="%scriptdir%..\deps-build\%build_dir%\openssl\include" ^
-DOPENSSL_CRYPTO_LIBRARY="%scriptdir%..\deps-build\%build_dir%\openssl\lib\libcrypto_a.lib" ^
-DOPENSSL_SSL_LIBRARY="%scriptdir%..\deps-build\%build_dir%\openssl\lib\libssl_a.lib" ^
-DCURL_INCLUDE_DIR="%scriptdir%..\deps-build\%build_dir%\curl\include" ^
-DCURL_LIBRARY="%scriptdir%..\deps-build\%build_dir%\curl\lib\libcurl_a.lib" ^
-DCMAKE_CXX_FLAGS="/D WIN32 /D _WINDOWS /EHsc /GR /W3 /Z7 /ZH:SHA_256 /guard:cf /Qspectre /sdl" ^
-DBUILD_TESTS=false ^
-DBUILD_SAMPLES=false

if %ERRORLEVEL% NEQ 0 goto :error

msbuild INSTALL.vcxproj /p:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

cd "%currdir%"
xcopy /S /E /I /Y /Q  %AZURE_CMAKE_BUILD_DIR%\%build_type%\azure-storage-lite.lib %AZURE_INSTALL_DIR%\lib\
xcopy /S /E /I /Y /Q  %AZURE_SOURCE_DIR%\include %AZURE_INSTALL_DIR%\include

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file azure %azure_version%
if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
exit /b 0

:error
cd "%currdir%"
exit /b 1
