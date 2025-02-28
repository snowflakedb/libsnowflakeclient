set tomlplusplus_version=3.4.0
call %*

:get-version
       set version=%tomlplusplus_version%
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

set DEPS_DIR=%scriptdir%..\deps
set TOMLPLUSPLUS_SOURCE_DIR=%DEPS_DIR%\tomlplusplus
set TOMLPLUSPLUS_INSTALL_DIR=%scriptdir%..\deps-build\%build_dir%\tomlplusplus

cd "%tomlplusplus_SOURCE_DIR%"

rd /S /Q %TOMLPLUSPLUS_INSTALL_DIR%
md %TOMLPLUSPLUS_INSTALL_DIR%
xcopy /s /e %TOMLPLUSPLUS_SOURCE_DIR%\include %TOMLPLUSPLUS_INSTALL_DIR%\include\

cd "%curdir%"

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file tomlplusplus %tomlplusplus_version%
if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
exit /b 0

:error
cd "%curdir%"
exit /b 1