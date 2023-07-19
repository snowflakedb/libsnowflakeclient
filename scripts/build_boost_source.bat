::
:: Build boost
:: GitHub repo: https://github.com/boostorg/boost.git
::
@echo off
set boost_version=1.75.0
call %*
goto :EOF

:get_version
    set version=%boost_version%
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

set curdir=%cd%

if "%platform%"=="x64" (
    set engine_dir=Program Files
	set bitness=64
)
if "%platform%"=="x86" (
    set engine_dir=Program Files (x86^)
	set bitness=32
)

::@echo off
set DEPS_DIR=%scriptdir%..\deps
set BOOST_SOURCE_DIR=%DEPS_DIR%\boost-%boost_version%
set BOOST_INSTALL_DIR=%scriptdir%..\deps-build\%build_dir%\boost

cd "%BOOST_SOURCE_DIR%"

rd /S /Q %BOOST_INSTALL_DIR%
md %BOOST_INSTALL_DIR%

if /I "%build_type%"=="debug" (
    set debugging=on
	set variant=debug
) else (
    set debugging=off
	set variant=release
)

if /I "%dynamic_runtime%"=="on" (
    set runtimelink=shared
) else (
    set runtimelink=static
)

call "%BOOST_SOURCE_DIR%\bootstrap.bat" --with-libraries=filesystem,regex,system
if %ERRORLEVEL% NEQ 0 goto :error
b2 stage --stagedir=%BOOST_INSTALL_DIR% --includedir=%BOOST_INSTALL_DIR%\include --address-model=64 --layout=system --with-system --with-filesystem --with-regex link=static runtime-link=%runtimelink% threading=multi address-model=%bitness% variant=%variant% runtime-debugging=%debugging% cxxflags=/ZH:SHA_256 install
if %ERRORLEVEL% NEQ 0 goto :error
::remove cmake files including local build path information
rd /S /Q %BOOST_INSTALL_DIR%\lib\cmake

cd "%curdir%"

echo === archiving the library
call "%scriptdir%utils.bat" :zip_file boost %boost_version%
if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
exit /b 0

:error
cd "%curdir%"
exit /b 1
