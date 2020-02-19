::
:: Build cmocka
::
@echo off
set cmocka_version=1.1.1
call %*
goto :EOF

:get_version
    set version=%cmocka_version%
    goto :EOF

:build
setlocal

set CMOCKA_DIR=cmocka-%cmocka_version%

set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

set curdir=%cd%

set target_name=cmocka_a.lib
call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%

echo === building cmocka

set INSTALL_DIR=%TMP%\cmocka
rd /S /Q "%INSTALL_DIR%"
md "%INSTALL_DIR%"

cd "%curdir%\deps\%CMOCKA_DIR%"
set cmake_dir=cmake-build-%arcdir%-%vs_version%-%build_type%
rd /S /Q %cmake_dir%
md %cmake_dir%
cd %cmake_dir%
cmake -G "%cmake_generator%" -A %cmake_architecture% ^
-DUNIT_TESTING=ON ^
-DCMAKE_BUILD_TYPE=%build_type% ^
-DCMAKE_C_FLAGS_DEBUG=/MTd /Zi /Ob0 /Od /RTC1 ^
-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG ^
-DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% ..
if %ERRORLEVEL% NEQ 0 goto :error
cmake --build . --config %build_type%
if %ERRORLEVEL% NEQ 0 goto :error

echo === staging cmocka
cd "%curdir%"
rmdir /q /s .\deps-build\%build_dir%\cmocka
md .\deps-build\%build_dir%\cmocka\include
md .\deps-build\%build_dir%\cmocka\lib
copy /v /y ^
    .\deps\%CMOCKA_DIR%\include\cmocka.h ^
    .\deps-build\%build_dir%\cmocka\include
copy /v /y ^
    .\deps\%CMOCKA_DIR%\include\cmocka_pbc.h ^
    .\deps-build\%build_dir%\cmocka\include
copy /v /y ^
    .\deps\%CMOCKA_DIR%\%cmake_dir%\src\%build_type%\cmocka.lib ^
    .\deps-build\%build_dir%\cmocka\lib\%target_name%

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file cmocka %cmocka_version%
if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
