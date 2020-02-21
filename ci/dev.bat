::
:: Set up dev 
::
@echo off
setlocal
set platform=x64
set build_type=Debug
set vs_version=VS14
echo === Setting up dev cmake directory for PLATFORM: %platform%, BUILD_TYPE: %build_type%, VS_VERSION: %vs_version%
echo === Adjust the parameter in this script for other configuration
set scriptdir=%~dp0
call %scriptdir%build.bat
