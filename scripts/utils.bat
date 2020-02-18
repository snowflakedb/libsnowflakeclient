::
:: Functions to use for building
::
@echo off
call %*
goto :EOF

:: VC16 is for Github Workflow windows-2019 virtual machine
:setup_visual_studio
    if /I "%~1"=="VS16" (
        if not "%VisualStudioVersion%"=="16.0" (
            echo === setting up the Visual Studio 16 environments
            call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" %arch%
            if %ERRORLEVEL% NEQ 0 goto :error
            echo === Done
        )
        goto :EOF
    )
    if /I "%~1"=="VS15" (
        if not "%VisualStudioVersion%"=="15.0" (
            echo === setting up the Visual Studio 15 environments
            call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" %arch%
            if %ERRORLEVEL% NEQ 0 goto :error
            echo === Done
        )
        goto :EOF
    )
    if /I "%~1"=="VS14" (
        if not "%VisualStudioVersion%"=="14.0" (
            echo === setting up the Visual Studio 14 environments
            call "%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch%
            if %ERRORLEVEL% NEQ 0 goto :error
            echo === Done
        )
        goto :EOF
    )
    echo off
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
    md artifacts
    call :get_zip_file_name %component_name% %component_version%
    del artifacts\%zip_file_name%
    set curdir=%cd%
    pushd deps-build\%build_dir%
        7z a %curdir%\artifacts\%zip_file_name% %component_name%
        7z l %curdir%\artifacts\%zip_file_name%
    popd
    if %ERRORLEVEL% NEQ 0 goto :error
    if defined GITHUB_ACTIONS (
        md %component_name%_artifacts
        copy /v /y artifacts\%zip_file_name% %component_name%_artifacts
    )
    goto :EOF

:check_directory
    setlocal
    set component_name=%~1
    If exist .\deps-build\%build_dir%\%component_name% (
        echo Exist .\deps-build\%build_dir%\%component_name%
        exit /b 0
    ) else (
        echo Not Exist .\deps-build\%build_dir%\%component_name%
        exit /b 1
    )
    goto :EOF

:init_git_variables
    echo === set GIT environment variables
    if "%GIT_URL%"=="" (
        set GIT_URL=https://github.com/snowflakedb/libsnowflakeclient.git
        if %ERRORLEVEL% NEQ 0 goto :error
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
    
:error
exit /b 1
