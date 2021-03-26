::
:: Build ARROW library
::
@echo off
set arrow_version=0.17.0
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
if defined GITHUB_ACTIONS (
    rd /S /Q C:\arrowlibs
    cd c:\
    7z x %dependencydir%\arrow_%arcdir%_%vsdir%_%build_type%-%arrow_version%.zip -oarrowlibs
    cd %dependencydir%
    mklink /j %build_dir%\arrow C:\arrowlibs\arrow
    mklink /j %build_dir%\arrow_deps C:\arrowlibs\arrow_deps
    mklink /j %build_dir%\boost C:\arrowlibs\boost
    del %dependencydir%\*.zip
    del %dependencydir%\*.gz
)
else (
    7z x %dependencydir%\arrow_%arcdir%_%vsdir%_%build_type%-%arrow_version%.zip -o%build_dir%
)
goto :success

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
