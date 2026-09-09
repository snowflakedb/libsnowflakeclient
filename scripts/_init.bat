::
:: Initialize variables
::

@echo off
echo === setting up global environment variables

set platform=%1
set build_type=%2
set vs_version=%3

:: normalize
if /I "%platform%"=="x64" set platform=x64
if /I "%platform%"=="x86" set platform=x86

set curdir=%cd%
set ARROW_FROM_SOURCE=1
set CJSON_VERSION=1.7.19
set CURL_VERSION=8.21.0

if defined arch (
    if not "%platform%"=="" (
        if not "%arch%"=="%platform%" (
	        echo The specified platform parameter doesn't match the existing arch. arch: %arch%, platform: %platform%
		    echo If you want to switch, restart Widnows terminal and run the command again.
	        goto :error
        )
    )
)

set arch=
set arcdir=
set cmake_architecture=

if /I "%platform%"=="x64" (
    set arch=x64
    set arcdir=win64
    set cmake_architecture=x64
)
if /I "%platform%"=="x86" (
    set arch=x86
    set arcdir=win32
    set cmake_architecture=Win32
)
if "%arch%"=="" (
    echo Specify PLATFORM to [x86, x64]
	goto :error
)

set target_name=
if /I "%build_type%"=="Debug" (
    set target_name=debug
)
if /I "%build_type%"=="Release" (
    set target_name=release
)

if "%target_name%"=="" (
    echo Specify BUILD_TYPE to [Debug, Release]
	goto :error
)

::Always use Visual Studio 2022
set cmake_generator=Visual Studio 17 2022
set vsdir=vs17
if "%VCINSTALLDIR%" == "" (
    if exist "c:\Program Files\Microsoft Visual Studio\2022\Community\VC" (
        set "VCINSTALLDIR=c:\Program Files\Microsoft Visual Studio\2022\Community\VC"
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC" (
        set "VCINSTALLDIR=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC" (
        set "VCINSTALLDIR=C:\Program Files ^(x86^)\Microsoft Visual Studio\2022\BuildTools\VC"
    ) else (
        set "VCINSTALLDIR="
        for /f "usebackq delims=" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VCINSTALLDIR=%%i\VC"
        if not defined VCINSTALLDIR (
            echo Set environment variable VCINSTALLDIR to sepecify Visual Studio 2022 install path.
            goto :error
        )
    )
)

if /I "%vs_version%"=="VS16" (
    set vsdir=vs16
)

if "%dynamic_runtime%"=="" (
	set dynamic_runtime=OFF
)

set build_dir=%arcdir%\%vsdir%\%build_type%

echo Building with platform: %platform%, build type: %build_type%, visual studio version: %vs_version%, cmake generator: %cmake_generator%

cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1

:: Look up VS2019 outside a parenthesized block. %ProgramFiles(x86)% and
:: vswhere's "[16.0,17.0)" both contain ')' and break cmd parsing inside IF ().
:find_vs16
set "PF86=%ProgramFiles(x86)%"
if exist "%PF86%\Microsoft Visual Studio\2019\Enterprise\VC" (
    set "VCINSTALLDIR=%PF86%\Microsoft Visual Studio\2019\Enterprise\VC"
    goto :EOF
)
if exist "%PF86%\Microsoft Visual Studio\2019\Professional\VC" (
    set "VCINSTALLDIR=%PF86%\Microsoft Visual Studio\2019\Professional\VC"
    goto :EOF
)
if exist "%PF86%\Microsoft Visual Studio\2019\Community\VC" (
    set "VCINSTALLDIR=%PF86%\Microsoft Visual Studio\2019\Community\VC"
    goto :EOF
)
if exist "%PF86%\Microsoft Visual Studio\2019\BuildTools\VC" (
    set "VCINSTALLDIR=%PF86%\Microsoft Visual Studio\2019\BuildTools\VC"
    goto :EOF
)
set "VCINSTALLDIR="
for /f "usebackq delims=" %%i in (`"%PF86%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -version "[16.0,17.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VCINSTALLDIR=%%i\VC"
goto :EOF
