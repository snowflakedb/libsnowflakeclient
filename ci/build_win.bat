::
:: Build libsnowflake and its dependencies
::

@echo off
setlocal
if not defined GITHUB_ACTIONS (
    set "path=C:\Program Files\7-Zip;C:\Python37;C:\python37\scripts;%path%"
)
set scriptdir=%~dp0
set curdir=%cd%

set utils_script="%scriptdir%..\scripts\utils.bat"
call %utils_script% :init_git_variables
if %ERRORLEVEL% NEQ 0 goto :error

set zlib_build_script="%scriptdir%..\scripts\build_zlib.bat"
set openssl_build_script="%scriptdir%..\scripts\build_openssl.bat"
set curl_build_script="%scriptdir%..\scripts\build_curl.bat"
set oob_build_script="%scriptdir%..\scripts\build_oob.bat"
set aws_build_script="%scriptdir%..\scripts\build_awssdk.bat"
set azure_build_script="%scriptdir%..\scripts\build_azuresdk.bat"
set cmocka_build_script="%scriptdir%..\scripts\build_cmocka.bat"
set libsnowflakeclient_build_script="%scriptdir%..\scripts\build_libsnowflakeclient.bat"

set upload_artifact_script="%scriptdir%container\upload_artifact.bat"

call :build %platform% %build_type% %vs_version% OFF

goto :EOF

:build
    @echo off
    setlocal
    set platform=%~1
    set build_type=%~2
    set vs_version=%~3
    set dynamic_runtime=%~4
    call "%scriptdir%..\scripts\_init.bat" %platform% %build_type% %vs_version%
    if %ERRORLEVEL% NEQ 0 goto :error
    call :download_build_component zlib "%zlib_build_script%" "%dynamic_runtime%"
    if %ERRORLEVEL% NEQ 0 goto :error
    call :download_build_component openssl "%openssl_build_script%" "%dynamic_runtime%"
    if %ERRORLEVEL% NEQ 0 goto :error
    call :download_build_component oob "%oob_build_script%" "%dynamic_runtime%"
    if %ERRORLEVEL% NEQ 0 goto :error
    call :download_build_component curl "%curl_build_script%" "%dynamic_runtime%"
    if %ERRORLEVEL% NEQ 0 goto :error
    call :download_build_component aws "%aws_build_script%" "%dynamic_runtime%"
    if %ERRORLEVEL% NEQ 0 goto :error
    call :download_build_component azure "%azure_build_script%" "%dynamic_runtime%"
    if %ERRORLEVEL% NEQ 0 goto :error
    call :download_build_component cmocka "%cmocka_build_script%" "%dynamic_runtime%"
    if %ERRORLEVEL% NEQ 0 goto :error
    call :build_component libsnowflakeclient "%libsnowflakeclient_build_script%" "%dynamic_runtime%"
    if %ERRORLEVEL% NEQ 0 goto :error
    exit /b 0

:download_build_component
    @echo off
    setlocal EnableDelayedExpansion
    set component_name=%~1
    set build_script=%~2
    set dynamic_runtime=%~3

    call %build_script% :get_version
    call %utils_script% :get_zip_file_name %component_name% %version%
    if defined GITHUB_ACTIONS (
        call %build_script% :build %platform% %build_type% %vs_version% %dynamic_runtime%
        if !ERRORLEVEL! NEQ 0 goto :error
    ) else (
        echo === download or build: %component_name% ===
        call %utils_script% :check_directory %component_name%
        if !ERRORLEVEL! EQU 0 (
            if "%build_clean%"=="false" (
                echo Skip download or build.
                exit /b 0
            )
        )
        rd /s /q deps-build\%arcdir%\%vsdir%\%build_type%\%component_name%
        cmd /c aws s3 cp --only-show-errors s3://sfc-dev1-data/dependency/%component_name%/%zip_file_name% %curdir%\artifacts\
        if !ERRORLEVEL! NEQ 0 (
            call %build_script% :build %platform% %build_type% %vs_version% %dynamic_runtime%
            if "%GIT_BRANCH%"=="origin/master" (
                :: upload the artifacts only for main
                call %utils_script% :upload_to_sfc_dev1_data %platform% %build_type% %vs_version% %component_name% %version%
                if !ERRORLEVEL! NEQ 0 goto :error
            )
        ) else (
            if not exist deps-build\%arcdir%\%vsdir%\%build_type% md deps-build\%arcdir%\%vsdir%\%build_type%
            pushd deps-build\%arcdir%\%vsdir%\%build_type%
                if !ERRORLEVEL! NEQ 0 goto :error
                rd /s /q %component_name%
                7z x "%curdir%\artifacts\%zip_file_name%"
            popd
        )
    )
    exit /b 0
    

:build_component
    @echo off
    setlocal EnableDelayedExpansion
    set component_name=%~1
    set build_script=%~2
    set dynamic_runtime=%~3

    echo === build: %component_name% ===
    call %build_script% :build %platform% %build_type% %vs_version% %dynamic_runtime% ON
    if %ERRORLEVEL% NEQ 0 goto :error

    call %build_script% :get_version
    if defined JENKINS_URL (
        echo === uploading ...
        call %utils_script% :upload_to_sfc_jenkins %platform% %build_type% %vs_version% %component_name% %version%
        if !ERRORLEVEL! NEQ 0 goto :error
        if "%GIT_BRANCH%"=="origin/master" (
            call %utils_script% :upload_to_sfc_dev1_data %platform% %build_type% %vs_version% %component_name% %version%
            if !ERRORLEVEL! NEQ 0 goto :error
        )
    )
    exit /b 0
    
:error:
exit /b 1

