if "%CLOUD_PROVIDER%"=="AWS" (
    gpg --quiet --batch --yes --decrypt --passphrase="%PARAMETERS_SECRET%" ^
      --output %scriptdir%../parameters.json ^
      %scriptdir%../.github/workflows/parameters_aws_capi.json.gpg
)
if "%CLOUD_PROVIDER%"=="AZURE" (
    gpg --quiet --batch --yes --decrypt --passphrase="%PARAMETERS_SECRET%" ^
      --output %scriptdir%../parameters.json ^
      %scriptdir%../.github/workflows/parameters_azure_capi.json.gpg
)
if "%CLOUD_PROVIDER%"=="GCP" (
    gpg --quiet --batch --yes --decrypt --passphrase="%PARAMETERS_SECRET%" ^
      --output %scriptdir%../parameters.json ^
      %scriptdir%../.github/workflows/parameters_gcp_capi.json.gpg
)
if defined CLOUD_PROVIDER (
    echo === Cloud Provider: %CLOUD_PROVIDER%
)