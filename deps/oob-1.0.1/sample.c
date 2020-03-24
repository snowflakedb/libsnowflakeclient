#include <stdio.h>
#include <string.h>
#include "oobtelemetry.h"

char data[]="[{                \"Created_On\":   \"2019-08-08 00:19:57\",                \"Name\": \"HttpRetryTimeout\",                \"SchemaVersion\":        1,                \"Tags\": {                        \"UUID\": \"ec6230e5-1ed7-4225-a6a0-e662e3a6e6a3\",                        \"connectionString\":     \"http://snowflake.reg.local:8082\",                        \"ctx_account\":  \"testaccount\",                        \"ctx_host\":     \"snowflake.reg.local\",                        \"ctx_port\":     \"8082\",                        \"ctx_protocol\": \"http\",                        \"ctx_user\":     \"snowman\",                        \"driver\":       \"ODBC\",                        \"version\":      \"2.19.10\",                        \"telemetryServerDeployment\":    \"dev\",                        \"snowhouseSchema\":      \"dev\"                },                \"Type\": \"Log\",                \"UUID\": \"ec6230e5-1ed7-4225-a6a0-e662e3a6e6a3\",                \"Urgent\":       false,                \"Value\":        {                        \"errorCode\":    502,                        \"exceptionMessage\":     \"HTTP error (http error) - code=502\"                },                \"request\":      \"http://snowflake.reg.local:8082/session/v1/login-request?requestId=c4d53986-ee7a-4f01-9fac-2604653e9c41&request_guid=abbab0e5-5c77-4102-9d29-d44efde6a050&databaseName=testdb&schemaName=testschema&warehouse=regress\"        }]";

int main(void)
{

  if (sendOOBevent(data) != 0 )
  {
    printf("Could not send oob event\n");
  }

  return 0;
  
}
