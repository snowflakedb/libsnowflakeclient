::
:: Build ARROW library
::
@echo off
set arrow_src_version=15.0.0
set arrow_build_version=3
:: The full version number for dependency packaging/uploading/downloading
if "%ARROW_FROM_SOURCE%"=="1" (
    set arrow_version=%arrow_src_version%.%arrow_build_version%
) else (
:: Use pre-build package of 0.17.1
    set arrow_version=0.17.1
)

call %*
goto :EOF

:get_version
    set version=%arrow_version%
    goto :EOF

:build
@echo off
setlocal
set platform=%1
set build_type=%2
set vs_version=%3
set dynamic_runtime=%4

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

set curdir=%cd%
set dependencydir=%scriptdir%..\deps-build\
cd %dependencydir%

rd /S /Q %build_dir%\arrow
rd /S /Q %build_dir%\arrow_deps
rd /S /Q %build_dir%\boost
if "%ARROW_FROM_SOURCE%"=="1" (
    echo "%scriptdir%build_boost_source.bat"
    call "%scriptdir%build_boost_source.bat" :build %platform% %build_type% %vs_version% %dynamic_runtime%
	if %ERRORLEVEL% NEQ 0 goto :error
	if /I "%platform%"=="x64" (
        call "%scriptdir%build_arrow_source.bat" :build %platform% %build_type% %vs_version% %dynamic_runtime%
        if %ERRORLEVEL% NEQ 0 goto :error
    )
) else (
    :: Temporarily hard-code vsdir in Arrow archive to vs14.
    7z x arrow_%arcdir%_vs14_%build_type%-0.17.1.zip -o%build_dir%
)
if defined GITHUB_ACTIONS (
    del %dependencydir%\*.zip
    del %dependencydir%\*.gz
)

cd "%curdir%"

echo === archiving the library
call "%scriptdir%utils.bat" :zip_files arrow %arrow_version% "arrow arrow_deps boost"
if %ERRORLEVEL% NEQ 0 goto :error

goto :success

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
