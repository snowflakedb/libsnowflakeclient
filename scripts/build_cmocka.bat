::
:: Build cmocka
::
@echo off
set CMOCKA_DIR=cmocka-1.1.1
set CMAKE_PROFILE=Visual Studio 14 2015

set platform=%1
set build_type=%2

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

if "%build_type%"=="Debug" (
    set target_name=cmocka_debug_a.lib
)
if "%build_type%"=="Release" (
    set target_name=cmocka_a.lib
)

if not "%VisualStudioVersion%"=="14.0" (
    echo === setting up the Visual Studio environments
    call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch%
    if %ERRORLEVEL% NEQ 0 goto :error
)

echo === building cmocka

set INSTALL_DIR=%TMP%\cmocka
rmdir /S /Q "%INSTALL_DIR%"
mkdir "%INSTALL_DIR%"

cd "%curdir%\deps\%CMOCKA_DIR%"
rmdir /S /Q cmake-build-%arcdir%
mkdir cmake-build-%arcdir%
cd cmake-build-%arcdir%
if "%arch%"=="x86" (
    cmake -G "%CMAKE_PROFILE%" ^
    -DUNIT_TESTING=ON ^
    -DCMAKE_BUILD_TYPE=%build_type% ^
    -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% ..
) else (
    cmake -G "%CMAKE_PROFILE%" -A %arch% ^
    -DUNIT_TESTING=ON ^
    -DCMAKE_BUILD_TYPE=%build_type% ^
    -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% ..
)
if %ERRORLEVEL% NEQ 0 goto :error
cmake --build . --config %build_type%
if %ERRORLEVEL% NEQ 0 goto :error

echo === staging cmocka
cd "%curdir%"
rmdir /q /s ^
    .\deps-build\%arcdir%\cmocka
mkdir .\deps-build\%arcdir%\cmocka
mkdir .\deps-build\%arcdir%\cmocka\include
mkdir .\deps-build\%arcdir%\cmocka\lib
copy /v /y ^
    .\deps\%CMOCKA_DIR%\include\cmocka.h ^
	.\deps-build\%arcdir%\cmocka\include
copy /v /y ^
    .\deps\%CMOCKA_DIR%\include\cmocka_pbc.h ^
	.\deps-build\%arcdir%\cmocka\include
copy /v /y ^
    .\deps\%CMOCKA_DIR%\cmake-build-%arcdir%\src\%build_type%\cmocka.lib ^
	.\deps-build\%arcdir%\cmocka\lib\%target_name%

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
