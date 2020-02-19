@echo off
call %*
goto :EOF

:sfc_jenkins
    setlocal
    set platform=%1
    set build_type=%2
    set vs_version=%3

    set component_name=%4
    set component_version=%5

    set scriptdir=%~dp0
    for /f "tokens=2 delims=/" %%a in ("%GIT_BRANCH%") do (
      set git_branch_base_name=%%a
    )

    call "%scriptdir%..\..\scripts\_init.bat" %platform% %build_type% %vs_version%
    call "%scriptdir%..\..\scripts\utils.bat" :get_zip_file_name %component_name% %component_version%

    @echo on
    md artfacts
    echo === downloading %zip_file_name% from s3://sfc-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/
    cmd /c aws s3 cp --only-show-errors s3://sfc-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/%zip_file_name% artifacts\
    if %ERRORLEVEL% NEQ 0 goto :error
    echo === downloading %zip_cmake_file_name% from s3://sfc-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/
    cmd /c aws s3 cp --only-show-errors s3://sfc-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/%zip_cmake_file_name% artifacts\
    if %ERRORLEVEL% NEQ 0 goto :error
    echo == 1
    cd "%curdir%"
    echo == 2
    exit /b 0

:error
echo == 3
cd "%curdir%"
echo == 4
exit /b 1
