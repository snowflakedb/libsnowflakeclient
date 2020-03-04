::
:: Test libsnowflake
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

set libsnowflakeclient_build_script="%scriptdir%..\scripts\build_libsnowflakeclient.bat"
set env_script="%scriptdir%..\scripts\env.bat"

call %utils_script% :set_parameters
if %ERRORLEVEL% NEQ 0 goto :error

call %env_script%
if %ERRORLEVEL% NEQ 0 goto :error

echo === setting test schema
if defined JENKINS_URL (
    set SNOWFLAKE_TEST_SCHEMA=JENKINS_%JOB_NAME:-=_%_%BUILD_NUMBER%
)
if defined GITHUB_ACTIONS (
    set SNOWFLAKE_TEST_SCHEMA=%RUNNER_TRACKING_ID:-=_%_%GITHUB_SHA%
)

if %ERRORLEVEL% NEQ 0 goto :error
call :init_python
if %ERRORLEVEL% NEQ 0 goto :error
call :create_schema
if %ERRORLEVEL% NEQ 0 goto :error

echo Test Schema: %SNOWFLAKE_TEST_SCHEMA%

call :test %platform% %build_type% %vs_version%
if %ERRORLEVEL% NEQ 0 goto :error
call :drop_schema
if %ERRORLEVEL% NEQ 0 goto :error
exit /b 0

:test
    @echo off
    setlocal
    set platform=%~1
    set build_type=%~2
    set vs_version=%~3

    call "%scriptdir%..\scripts\_init.bat" %platform% %build_type% %vs_version%
    if %ERRORLEVEL% NEQ 0 goto :error
    call :test_component libsnowflakeclient "%libsnowflakeclient_build_script%"
    if %ERRORLEVEL% NEQ 0 goto :error
    exit /b 0

:test_component
    @echo off
    setlocal EnableDelayedExpansion
    set component_name=%~1
    set build_script=%~2
    
    set cmake_dir=cmake-build-%arcdir%-%vs_version%-%build_type%
    call %utils_script% :get_zip_file_name %component_name% %component_version%
    call %build_script% :get_version
    if defined JENKINS_URL (
        call %utils_script% :download_from_sfc_jenkins %platform% %build_type% %vs_version% %component_name% %version%
        if !ERRORLEVEL! NEQ 0 goto :error
        dir artifacts

        7z x "%curdir%\artifacts\%zip_cmake_file_name%"
        if !ERRORLEVEL! NEQ 0 goto :error
    )
    pushd %cmake_dir%
        ctest -V -E "valgrind.*"
        if %ERRORLEVEL% NEQ 0 (
            call :drop_schema
            goto :error
        )
    popd
    exit /b 0

:init_python
    @echo off
    echo === creating venv
    python -m venv venv
    call venv\scripts\activate
    python -m pip install -U pip > nul 2>&1
    if %ERRORLEVEL% NEQ 0 goto :error
    pip install snowflake-connector-python > nul 2>&1
    if %ERRORLEVEL% NEQ 0 goto :error
    exit /b 0

:create_schema
    @echo off
    echo === creating schema
    pushd scripts
        python create_schema.py
        if %ERRORLEVEL% NEQ 0 goto :error
    popd
    exit /b 0

:drop_schema
    @echo off
    echo === dropping schema
    pushd scripts
        python drop_schema.py
        if %ERRORLEVEL% NEQ 0 goto :error
    popd
    exit /b 0

:error
exit /b 1
