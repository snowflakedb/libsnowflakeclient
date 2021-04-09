::
:: Build ARROW library
::
@echo off
set arrow_version=0.17.1
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

set scriptdir=%~dp0
call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error

set curdir=%cd%
set dependencydir=%scriptdir%..\deps-build\
cd %dependencydir%

rd /S /Q %build_dir%\arrow
rd /S /Q %build_dir%\arrow_deps
rd /S /Q %build_dir%\boost
:: Temporarily hard-code vsdir in Arrow archive to vs14.
7z x arrow_%arcdir%_vs14_%build_type%-%arrow_version%.zip -o%build_dir%
if defined GITHUB_ACTIONS (
    del %dependencydir%\*.zip
    del %dependencydir%\*.gz
)
goto :success

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
