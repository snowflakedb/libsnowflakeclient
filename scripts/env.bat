::
:: Set the environment variables for tests
::

if not exist parameters.appveyor.json (
    echo "parameters.appveyor.json doesn't exist"
    exit /b 2
)

echo @echo off>parameter.bat
jq -r ".testconnection | to_entries | map(\"set \(.key)=\(.value)\") | .[]" parameters.appveyor.json >> parameter.bat
call parameter.bat
