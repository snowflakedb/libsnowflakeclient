@echo off
call %*
goto :EOF

:sfc_dev1_data
    setlocal
    set platform=%1
    set build_type=%2
    set vs_version=%3

    set component_name=%4
    set component_version=%5

    set scriptdir=%~dp0

    call "%scriptdir%..\..\scripts\_init.bat" %platform% %build_type% %vs_version%
    call "%scriptdir%..\..\scripts\utils.bat" :get_zip_file_name %component_name% %component_version%

    echo === uploading %component_name% to s3://sfc-dev1-data/dependency/%component_name%/%zip_file_name%
    cmd /c aws s3 cp --only-show-errors artifacts\%zip_file_name% s3://sfc-dev1-data/dependency/%component_name%/
    if %ERRORLEVEL% NEQ 0 goto :error
    cd "%curdir%"
    exit /b 0
    
:sfc_jenkins
    @echo on
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

    set target_path=s3://sfc-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/
    echo === uploading artifacts\%zip_file_name% to %target_path%
    cmd /c aws s3 cp --only-show-errors artifacts\%zip_file_name% %target_path%
    if %ERRORLEVEL% NEQ 0 goto :error
    echo === uploading artifacts\%zip_cmake_file_name% to %target_path%
    cmd /c aws s3 cp --only-show-errors artifacts\%zip_cmake_file_name% %target_path%
    if %ERRORLEVEL% NEQ 0 goto :error
    set parent_target_path=s3://sfc-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/
    echo === uploading latest_commit to %parent_target_path%
    echo %GIT_COMMIT%>latest_commit
    cmd /c aws s3 cp --only-show-errors latest_commit %parent_target_path%
    cd "%curdir%"
    exit /b 0

:error
cd "%curdir%"
exit /b 1
