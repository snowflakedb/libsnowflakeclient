::
:: Initialize variables
::

echo === setting up global environment variables

set platform=%1
set build_type=%2

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
if "%platform%"=="x64" (
    set arch=x64
    set arcdir=win64
)
if "%platform%"=="x86" (
    set arch=x86
    set arcdir=win32
)
if "%arch%"=="" (
    echo Specify the architecture. [x86, x64]
	goto :error
)

set target_name=
if "%build_type%"=="Debug" (
    set target_name=debug
)
if "%build_type%"=="Release" (
    set target_name=relese
)

if "%target_name%"=="" (
    echo Specify the build type. [Debug, Release]
	goto :error
)

cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1