::
:: Test libsnowflake
::
@echo on
setlocal
python --version

set platform=%1
set build_type=%2
set vs_version=%3

set scriptdir=%~dp0
call "%scriptdir%..\scripts\_init.bat" %platform% %build_type% %vs_version%
@echo on

set cmake_dir=cmake-build-%arcdir%-%vs_version%-%build_type%

%cmake_dir%\tests\%build_type%\test_unit_logger
%cmake_dir%\tests\%build_type%\test_unit_connect_parameters.exe
