::
:: Run Tests
::

echo Setting the enviroment variables for tests
call .\scripts\env.bat

echo %SNOWFLAKE_TEST_ACCOUNT%

echo ==> ex_connect
.\cmake-build\examples\Release\ex_connect

echo ==> ex_large_result_set
.\cmake-build\examples\Release\ex_large_result_set

::for /r ".\cmake-build\examples\Release" %%a in (*.exe) do (
::    echo ==> %%~fa
::    %%~fa
::)
