::
:: Test libsnowflake
::

@echo on
setlocal
if not defined GITHUB_ACTIONS (
    set path=C:\Program Files\7-Zip;%path%
    set path=C:\Python37;C:\python37\scripts;%path%
)
set scriptdir=%~dp0
set curdir=%cd%
set utils_script="%scriptdir%..\scripts\utils.bat"

call %utils_script% :init_git_variables
if %ERRORLEVEL% NEQ 0 goto :error

set libsnowflakeclient_build_script="%scriptdir%..\scripts\build_libsnowflakeclient.bat"
set download_artifact_script="%scriptdir%container\download_artifact.bat"
set env_script="%scriptdir%..\scripts\env.bat"

call %scriptdir%scripts\set_parameters.bat
call %env_script%
if %ERRORLEVEL% NEQ 0 goto :error

echo === setting test schema
if defined JOB_NAME (
    set SNOWFLAKE_TEST_SCHEMA=JENKINS_%JOB_NAME:-=_%_%BUILD_NUMBER%
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
    setlocal
    set component_name=%~1
    set build_script=%~2
    
    set cmake_dir=cmake-build-%arcdir%-%vs_version%-%build_type%
    if not defined GITHUB_ACTIONS (
        call %build_script% :get_version
        call %download_artifact_script% :sfc_jenkins %platform% %build_type% %vs_version% %component_name% %version%
        if %ERRORLEVEL% NEQ 0 goto :error
        dir artifacts
        call %utils_script% :get_zip_file_name %component_name% %component_version%

        7z x "%curdir%\artifacts\%zip_cmake_file_name%"
        if %ERRORLEVEL% NEQ 0 goto :error
    )
    pushd %cmake_dir%
         ctest -V -E "valgrind.*"
    popd
    exit /b 0

:init_python
    @echo off
    echo === creating venv
    python -m venv venv
    call venv\scripts\activate
    python -m pip install -U pip > nul 2>&1
    pip install snowflake-connector-python > nul 2>&1
    exit /b 0

:create_schema
    @echo off
    echo === creating schema
    pushd scripts
        python create_schema.py
    popd
    exit /b 0

:drop_schema
    @echo off
    echo === dropping schema
    pushd scripts
        python drop_schema.py
    popd
    exit /b 0

:error
exit /b 1
