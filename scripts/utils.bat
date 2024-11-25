::
:: Functions to use for building
::
@echo off
call %*
goto :EOF

:: VC16 is for Github Workflow windows-2019 virtual machine
:setup_visual_studio
    if /I "%~1"=="VS17" (
        if not "%VisualStudioVersion%"=="17.0" (
            echo === setting up the Visual Studio 17 environments
            call "%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat" %arch%
        )
        goto :EOF
    )
    if /I "%~1"=="VS16" (
        if not "%VisualStudioVersion%"=="16.0" (
            echo === setting up the Visual Studio 16 environments
            call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" %arch%
        )
        goto :EOF
    )
    if /I "%~1"=="VS15" (
        if not "%VisualStudioVersion%"=="15.0" (
            echo === setting up the Visual Studio 15 environments
            call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" %arch%
        )
        goto :EOF
    )
    if /I "%~1"=="VS14" (
        if not "%VisualStudioVersion%"=="14.0" (
            echo === setting up the Visual Studio 14 environments
            call "%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch%
            :: installation of vs 14 on new jenkins nodes causes switching current directory - let's go back to workspace
            set curdir=%WORKSPACE%
            cd %curdir%
        )
        goto :EOF
    )
    if not defined VisualStudioVersion (
        echo === ERROR: no VisualStudioVersion is set. %~1
        goto :error
    )
    goto :EOF

:get_zip_file_name
    set component_name=%~1
    set component_version=%~2
    if "%component_name%"=="" (
        echo Set component [zlib, openssl, curl, aws, azure, cmocka, libsnowflakeclient]
        goto :error
    )
    if "%component_version%"=="" (
        echo Set component_version
        goto :error
    )
    if "%arcdir%"=="" (
        echo Set arcdir [win32, win64]
        goto :error
    )
    if "%vsdir%"=="" (
        echo Set vsdir [vs14, vs15]
        goto :error
    )
    if "%build_type%"=="" (
        echo Set build_type [Debug, Release]
        goto :error
    )
    set zip_file_name=%component_name%_%arcdir%_%vsdir%_%build_type%-%component_version%.zip
    set zip_cmake_file_name=%component_name%_cmake_%arcdir%_%vsdir%_%build_type%-%component_version%.zip
    echo === zip_file_name: %zip_file_name%
    if "%component_name%"=="libsnowflakeclient" echo === zip_cmake_file_name: %zip_cmake_file_name%
    goto :EOF

:zip_file
    setlocal
    set component_name=%~1
    set component_version=%~2
    if not exist artifacts md artifacts
    call :get_zip_file_name %component_name% %component_version%
    del artifacts\%zip_file_name%
    set curdir=%cd%
    pushd deps-build\%build_dir%
        7z a %curdir%\artifacts\%zip_file_name% %component_name%
        7z l %curdir%\artifacts\%zip_file_name%
    popd
    if %ERRORLEVEL% NEQ 0 goto :error
    goto :EOF

:zip_files
    setlocal
    set component_name=%~1
    set component_version=%~2
    set files=%~3
    if not exist artifacts md artifacts
    call :get_zip_file_name %component_name% %component_version%
    del artifacts\%zip_file_name%
    set curdir=%cd%
    pushd deps-build\%build_dir%
        7z a %curdir%\artifacts\%zip_file_name% %files%
        7z l %curdir%\artifacts\%zip_file_name%
    popd
    if %ERRORLEVEL% NEQ 0 goto :error
    goto :EOF

:check_directory
    setlocal
    set component_name=%~1
    If exist deps-build\%build_dir%\%component_name% (
        echo Exist deps-build\%build_dir%\%component_name%
        goto :EOF
    ) else (
        echo Not Exist deps-build\%build_dir%\%component_name%
        exit /b 1
    )
    goto :EOF

:init_git_variables
    echo === set GIT environment variables
    if "%GIT_URL%"=="" (
        set GIT_URL=https://github.com/snowflakedb/libsnowflakeclient.git
    )
    if "%GIT_BRANCH%"=="" (
        for /f "delims=" %%A in ('git rev-parse --abbrev-ref HEAD') do @set GIT_BRANCH=origin/%%A
        if %ERRORLEVEL% NEQ 0 goto :error
    )
    if "%GIT_COMMIT%"=="" (
        for /f "delims=" %%A in ('git rev-parse HEAD') do @set GIT_COMMIT=%%A
        if %ERRORLEVEL% NEQ 0 goto :error
    )
    echo GIT_BRANCH: %GIT_BRANCH%, GIT_COMMIT: %GIT_COMMIT%
    goto :EOF

:upload_to_sfc_dev1_data
    setlocal
    set platform=%1
    set build_type=%2
    set vs_version=%3

    set component_name=%4
    set component_version=%5

    set scriptdir=%~dp0

    call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
    call :get_zip_file_name %component_name% %component_version%

    echo === uploading %component_name% to s3://sfc-eng-data/dependency/%component_name%/%zip_file_name%
    cmd /c aws s3 cp --only-show-errors artifacts\%zip_file_name% s3://sfc-eng-data/dependency/%component_name%/
    if %ERRORLEVEL% NEQ 0 goto :error
    cd "%curdir%"
    goto :EOF

:upload_to_sfc
:upload_to_sfc_jenkins
    @echo off
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

    call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
    call :get_zip_file_name %component_name% %component_version%

    set target_path=s3://sfc-eng-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/
    echo === uploading artifacts\%zip_file_name% to %target_path%
    cmd /c aws s3 cp --only-show-errors artifacts\%zip_file_name% %target_path%
    if %ERRORLEVEL% NEQ 0 goto :error
    echo === uploading artifacts\%zip_cmake_file_name% to %target_path%
    cmd /c aws s3 cp --only-show-errors artifacts\%zip_cmake_file_name% %target_path%
    if %ERRORLEVEL% NEQ 0 goto :error
    set parent_target_path=s3://sfc-eng-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/
    echo === uploading latest_commit to %parent_target_path%
    echo %GIT_COMMIT%>latest_commit
    cmd /c aws s3 cp --only-show-errors latest_commit %parent_target_path%
    cd "%curdir%"
    goto :EOF

:download_from_sfc_jenkins
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

    call "%scriptdir%_init.bat" %platform% %build_type% %vs_version%
    call :get_zip_file_name %component_name% %component_version%

    md artfacts
    echo === downloading %zip_file_name% from s3://sfc-eng-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/
    cmd /c aws s3 cp --only-show-errors s3://sfc-eng-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/%zip_file_name% artifacts\
    if %ERRORLEVEL% NEQ 0 goto :error
    echo === downloading %zip_cmake_file_name% from s3://sfc-eng-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/
    cmd /c aws s3 cp --only-show-errors s3://sfc-eng-jenkins/repository/%component_name%/%arcdir%/%git_branch_base_name%/%GIT_COMMIT%/%zip_cmake_file_name% artifacts\
    if %ERRORLEVEL% NEQ 0 goto :error
    cd "%curdir%"
    goto :EOF

:set_parameters
    setlocal EnableDelayedExpansion
    @echo off
    set scriptdir=%~dp0

    if /I "%CLOUD_PROVIDER%"=="AWS" (
        echo == AWS
        gpg --quiet --batch --yes --decrypt --passphrase="%PARAMETERS_SECRET%" ^
          --output %scriptdir%..\parameters.json ^
          %scriptdir%..\.github\workflows\parameters_aws_capi.json.gpg
        if !ERRORLEVEL! NEQ 0 goto :error
        endlocal
        set CLOUD_PROVIDER=AWS
    )
    if /I "%CLOUD_PROVIDER%"=="AZURE" (
        echo == AZURE
        gpg --quiet --batch --yes --decrypt --passphrase="%PARAMETERS_SECRET%" ^
          --output %scriptdir%..\parameters.json ^
          %scriptdir%..\.github\workflows\parameters_azure_capi.json.gpg
        if !ERRORLEVEL! NEQ 0 goto :error
        endlocal
        set CLOUD_PROVIDER=AZURE
    )
    if /I "%CLOUD_PROVIDER%"=="GCP" (
        echo === GCP
        gpg --quiet --batch --yes --decrypt --passphrase="%PARAMETERS_SECRET%" ^
          --output %scriptdir%..\parameters.json ^
          %scriptdir%..\.github\workflows\parameters_gcp_capi.json.gpg
        if !ERRORLEVEL! NEQ 0 goto :error
        endlocal
        set CLOUD_PROVIDER=GCP
    )
    if defined CLOUD_PROVIDER (
        echo === Cloud Provider: %CLOUD_PROVIDER%
    ) else (
        echo === No CLOUD_PROVIDER is set.
    )
    goto :EOF

:error
exit /b 1
