
set picojson_version=1.3.0
call %*

:get-version
       set version=%picojson_version%
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
set PICOJSON_SOURCE_DIR=%DEPS_DIR%\picojson
set PICOJSON_INSTALL_DIR=%scriptdir%..\deps-build\%build_dir%\picojson

cd "%PICOJSON_SOURCE_DIR%"

rd /S /Q %PICOJSON_INSTALL_DIR%
md %PICOJSON_INSTALL_DIR%
md %PICOJSON_INSTALL_DIR%\include
copy %PICOJSON_SOURCE_DIR%\picojson.h %PICOJSON_INSTALL_DIR%\include

cd "%currdir%"

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file picojson %picojson_version%
if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
exit /b 0

:error
cd "%currdir%"
exit /b 1
