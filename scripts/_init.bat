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

set cmake_generator=
set vsdir=
if /I "%vs_version%"=="VS16" (
    set cmake_generator=Visual Studio 16 2019
    set vsdir=vs16
)
if /I "%vs_version%"=="VS15" (
    set cmake_generator=Visual Studio 15 2017
    set vsdir=vs15
)
if /I "%vs_version%"=="VS14" (
    set cmake_generator=Visual Studio 14 2015
    set vsdir=vs14
)
if "%cmake_generator%"=="" (
    echo Specify the VS_VERSION to the Visual Studio Version [VS16, VS15, VS14]
    goto :error
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