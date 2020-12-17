::
:: Build Apache Arrow
::
@echo off
set arrow_version=0.17.0
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
set build_with_md=%4

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
call "%scriptdir%utils.bat" :setup_visual_studio %vs_version%%
if %ERRORLEVEL% NEQ 0 goto :error

set curdir=%cd%

if "%platform%"=="x64" (
    set engine_dir=Program Files
)
if "%platform%"=="x86" (
    set engine_dir=Program Files (x86^)
)

@echo off
set ARROW_SOURCE_DIR=%scriptdir%..\deps\arrow
set ARROW_CMAKE_BUILD_DIR=%ARROW_SOURCE_DIR%\cmake-build-%arcdir%-%vs_version%-%build_type%
set ARROW_INSTALL_DIR=%scriptdir%..\deps-build\%build_dir%\arrow

:: Until a proper build script can be written for Arrow, download from from SFC.
set artifactsdir=%~dp0..\artifacts
set dependencydir=%~dp0..\deps-build\%build_dir%\%component_name

call "%scriptdir%utils.bat" :download_from_sfc_dev1_data %platform% %build_type% %vs_version% arrow %arrow_version%

7z x -y -bd %artifactsdir%\%zip_file_name% -o%dependencydir%
del %artifactsdir%\%zip_file_name%

if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
exit /b 0

:error
cd "%curdir%"
exit /b 1
