::
:: Remap libsnowflakeclient build output into the pdo_snowflake vendor layout.
:: Usage: scripts\package_for_pdo.bat --platform win64-vs17|win64-vs16 [--output pdo-vendor]
::
@echo off
python "%~dp0package_for_pdo.py" package %*
if %ERRORLEVEL% NEQ 0 exit /b 1
