::
:: Run Tests
::

echo Setting the enviroment variables for tests
call .\scripts\env.bat

echo %SNOWFLAKE_TEST_ACCOUNT%
set SNOWFLAKE_TEST_CA_BUNDLE_FILE=%cd%\cacert.pem

for /r ".\cmake-build\examples\Release" %%a in (*.exe) do (
    echo ==> %%~fa
    %%~fa
)
