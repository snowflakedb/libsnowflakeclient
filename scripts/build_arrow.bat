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

rd /S /Q arrow
rd /S /Q arrow_deps
rd /S /Q boost
7z x %dependencydir%\arrow_%arcdir%_%vsdir%_%build_type%-%arrow_version%.zip

rd /S /Q %build_dir%\arrow
rd /S /Q %build_dir%\arrow_deps
rd /S /Q %build_dir%\boost
copy arrow %build_dir%
copy arrow_deps %build_dir%
copy boost %build_dir%

goto :success

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
