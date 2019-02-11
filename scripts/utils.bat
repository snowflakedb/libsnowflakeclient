::
:: Functions to use for building
::
@echo off
call %*
goto :EOF
:setup_visual_studio
	if "%~1"=="VS15" (
        if not "%VisualStudioVersion%"=="15.0" (
            echo === setting up the Visual Studio 15 environments
            call "c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" %arch%
            if %ERRORLEVEL% NEQ 0 goto :error
        )
    ) else (
        if not "%VisualStudioVersion%"=="14.0" (
            echo === setting up the Visual Studio 14 environments
            call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %arch%
            if %ERRORLEVEL% NEQ 0 goto :error
        )
    )
	goto :EOF
