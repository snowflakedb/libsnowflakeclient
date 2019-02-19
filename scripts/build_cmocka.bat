::
:: Build cmocka
::
@echo off
set CMOCKA_DIR=cmocka-1.1.1

set platform=%1
set build_type=%2
set vs_version=%3

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

set target_name=cmocka_a.lib
call "%scriptdir%\utils.bat" :setup_visual_studio %vs_version%

echo === building cmocka

set INSTALL_DIR=%TMP%\cmocka
rmdir /S /Q "%INSTALL_DIR%"
mkdir "%INSTALL_DIR%"

cd "%curdir%\deps\%CMOCKA_DIR%"
rmdir /S /Q cmake-build-%arcdir%
mkdir cmake-build-%arcdir%
cd cmake-build-%arcdir%
if "%arch%"=="x86" (
    cmake -G "%cmake_generator%" ^
    -DUNIT_TESTING=ON ^
    -DCMAKE_BUILD_TYPE=%build_type% ^
    -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% ..
) else (
    cmake -G "%cmake_generator%" -A %arch% ^
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
    .\deps-build\%build_dir%\cmocka
mkdir .\deps-build\%build_dir%\cmocka
mkdir .\deps-build\%build_dir%\cmocka\include
mkdir .\deps-build\%build_dir%\cmocka\lib
copy /v /y ^
    .\deps\%CMOCKA_DIR%\include\cmocka.h ^
	.\deps-build\%build_dir%\cmocka\include
copy /v /y ^
    .\deps\%CMOCKA_DIR%\include\cmocka_pbc.h ^
	.\deps-build\%build_dir%\cmocka\include
copy /v /y ^
    .\deps\%CMOCKA_DIR%\cmake-build-%arcdir%\src\%build_type%\cmocka.lib ^
	.\deps-build\%build_dir%\cmocka\lib\%target_name%

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
