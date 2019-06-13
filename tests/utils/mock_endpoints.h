/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_MOCK_ENDPOINTS_H
#define SNOWFLAKECLIENT_MOCK_ENDPOINTS_H

#ifdef __cplusplus
extern "C" {
#endif

// Common mock strings shared across tests
#define MOCK_HEADER_AUTH_TOKEN "Authorization: Snowflake Token=\"ETMsDgAAAVHApArOABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEH6XI1dGt/WBW5OrD4pFJF0AAABwUr+kbG6lEvgdJ1unUV69p3ZQWhMXh13TrFu2OXIA19TnaNFoFe7L0GOUASskpvLxBwNjRITjaO3cDBZnZ5Kthv7Vu6wPXwPp9vtXtWKzy+B58arDdXoF7ktIjNhGZWnGlvrEM0IOUBXVmO5jHdKUZwAU1T3TJfuZgjJNs2gKQtbm2+AW7Yg=\""
#define MOCK_HEADER_SERVICE_NAME "X-Snowflake-Service: fakeservicename"
#define MOCK_URL_DELETE_CONNECTION_STANDARD "https://standard.snowflakecomputing.com:443/session"
#define MOCK_REQUEST_TYPE_POST "POST"
#define MOCK_RESPONSE_DELETE_CONNECTION "{\n \
                  \"code\": null,\n \
                  \"data\": null,\n \
                  \"message\": null,\n \
                  \"success\": true\n \
                }"

// Standard Login
#define MOCK_URL_STANDARD_LOGIN "https://standard.snowflakecomputing.com:443/session/v1/login-request"
#define MOCK_BODY_STANDARD_LOGIN "{\n\t\"data\":\t{\n\t\t\"CLIENT_APP_ID\":\t\"C API\",\n\t\t\"CLIENT_APP_VERSION\":\t\"0.0.0\",\n\t\t\"ACCOUNT_NAME\":\t\"standard\",\n\t\t\"LOGIN_NAME\":\t\"standarduser\",\n\t\t\"PASSWORD\":\t\"secret-password\",\n\t\t\"CLIENT_ENVIRONMENT\":\t{\n\t\t\t\"APPLICATION\":\t\"C API\",\n\t\t\t\"OS\":\t\"Linux\",\n\t\t\t\"OS_VERSION\":\t\"0\"\n\t\t},\n\t\t\"SESSION_PARAMETERS\":\t{\n\t\t\t\"AUTOCOMMIT\":\t\"TRUE\",\n\t\t\t\"TIMEZONE\":\t\"UTC\"\n\t\t}\n\t}\n}"
#define MOCK_RESPONSE_STANDARD_LOGIN "{\n \
                  \"code\": null,\n \
                  \"data\":\n \
                    {\n \
                      \"displayUserName\": \"USERNAME\",\n \
                      \"firstLogin\": false,\n \
                      \"healthCheckInterval\": 45,\n \
                      \"masterToken\": \"ETMsDgAAAVHApAq4ABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEFXybqPxPmJrBv1S2+TBVY0AAACgxIkcbg5ZTa3xHwhdUQ1kBjEbV1YJcYuG0ITQ8/jH37FhSO4UYHJCZ/TV0+CF5TBA+a4nkmyh1INKk6ndi7vWp1hZGM7rUtRkZMmdjnUTPKBZpXd8LFiGfofEus8K4OPEmf3PPvwrxMvGbzxNCTAHo/hiD8sVNkSjVXfPPJKwO7HHvnPuNP4LzpOf0mqqIWDHPCHYZjlsUOLM+f2CtKCgIgAUvY30vA343bf1eXUNqlCvH45kEVU=\",\n \
                      \"masterValidityInSeconds\": 14400,\n \
                      \"newClientForUpgrade\": null,\n \
                      \"remMeToken\": \"ETMsDgAAAVHApAq4ABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEFXybqPxPmJrBv1S2+TBVY0AAACgxIkcbg5ZTa3xHwhdUQ1kBjEbV1YJcYuG0ITQ8/jH37FhSO4UYHJCZ/TV0+CF5TBA+a4nkmyh1INKk6ndi7vWp1hZGM7rUtRkZMmdjnUTPKBZpXd8LFiGfofEus8K4OPEmf3PPvwrxMvGbzxNCTAHo/hiD8sVNkSjVXfPPJKwO7HHvnPuNP4LzpOf0mqqIWDHPCHYZjlsUOLM+f2CtKCgIgAUvY30vA343bf1eXUNqlCvH45kEVU=\",\n \
                      \"remMeValidityInSeconds\": 14400,\n \
                      \"serverVersion\": \"Dev\",\n \
                      \"sessionId\": \"51539800306\",\n \
                      \"token\": \"ETMsDgAAAVHApArOABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEH6XI1dGt/WBW5OrD4pFJF0AAABwUr+kbG6lEvgdJ1unUV69p3ZQWhMXh13TrFu2OXIA19TnaNFoFe7L0GOUASskpvLxBwNjRITjaO3cDBZnZ5Kthv7Vu6wPXwPp9vtXtWKzy+B58arDdXoF7ktIjNhGZWnGlvrEM0IOUBXVmO5jHdKUZwAU1T3TJfuZgjJNs2gKQtbm2+AW7Yg=\",\n \
                      \"validityInSeconds\": 3600,\n \
                      \"parameters\": [{\n \
                        \"name\": \"TIMEZONE\",\n \
                        \"value\": \"America/Los_Angeles\"\n \
                      }, {\n \
                        \"name\": \"TIMESTAMP_OUTPUT_FORMAT\",\n \
                        \"value\": \"DY, DD MON YYYY HH24:MI:SS TZHTZM\"\n \
                      }, {\n \
                        \"name\": \"TIMESTAMP_NTZ_OUTPUT_FORMAT\",\n \
                        \"value\": \"\"\n \
                      }, {\n \
                        \"name\": \"TIMESTAMP_LTZ_OUTPUT_FORMAT\",\n \
                        \"value\": \"\"\n \
                      }, {\n \
                        \"name\": \"TIMESTAMP_TZ_OUTPUT_FORMAT\",\n \
                        \"value\": \"\"\n \
                      }, {\n \
                        \"name\": \"DATE_OUTPUT_FORMAT\",\n \
                        \"value\": \"YYYY-MM-DD\"\n \
                      }, {\n \
                        \"name\": \"TIME_OUTPUT_FORMAT\",\n \
                        \"value\": \"HH24:MI:SS\"\n \
                      }, {\n \
                        \"name\": \"CLIENT_RESULT_PREFETCH_SLOTS\",\n \
                        \"value\": 2\n \
                      }, {\n \
                        \"name\": \"CLIENT_RESULT_PREFETCH_THREADS\",\n \
                        \"value\": 1\n \
                      }, {\n \
                        \"name\": \"CLIENT_HONOR_CLIENT_TZ_FOR_TIMESTAMP_NTZ\",\n \
                        \"value\": true\n \
                      }, {\n \
                        \"name\": \"CLIENT_USE_V1_QUERY_API\",\n \
                        \"value\": true\n \
                      }, {\n \
                        \"name\": \"CLIENT_DISABLE_INCIDENTS\",\n \
                        \"value\": true\n \
                      }, {\n \
                        \"name\": \"CLIENT_SESSION_KEEP_ALIVE\",\n \
                        \"value\": false\n \
                      }, {\n \
                        \"name\": \"CLIENT_SESSION_KEEP_ALIVE_HEARTBEAT_FREQUENCY\",\n \
                        \"value\": 3600\n \
                      }]\n \
                    },\n \
                  \"message\": null,\n \
                  \"success\": true\n \
                }"

// Standard query sending
#define MOCK_URL_STANDARD_QUERY "https://standard.snowflakecomputing.com:443/queries/v1/query-request"
#define MOCK_BODY_STANDARD_QUERY "{\n\t\"sqlText\":\t\"select 1;\",\n\t\"asyncExec\":\tfalse,\n\t\"sequenceId\":\t1,\n\t\"querySubmissionTime\":\t0\n}"
#define MOCK_RESPONSE_STANDARD_QUERY "{\n \
                  \"data\":\n \
                    {\n \
                      \"parameters\":\n \
                        [\n \
                          {\n \
                            \"name\": \"DATE_OUTPUT_FORMAT\",\n \
                            \"value\": \"YYYY-MM-DD\"\n \
                          },\n \
                          {\n \
                            \"name\": \"CLIENT_USE_V1_QUERY_API\",\n \
                            \"value\": true\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMESTAMP_LTZ_OUTPUT_FORMAT\",\n \
                            \"value\": \"\"\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMESTAMP_NTZ_OUTPUT_FORMAT\",\n \
                            \"value\": \"\"\n \
                          },\n \
                          {\n \
                            \"name\": \"CLIENT_RESULT_PREFETCH_THREADS\",\n \
                            \"value\": 1\n \
                          },\n \
                          {\n \
                            \"name\": \"CLIENT_HONOR_CLIENT_TZ_FOR_TIMESTAMP_NTZ\",\n \
                            \"value\": true\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMEZONE\",\n \
                            \"value\": \"America/Los_Angeles\"\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMESTAMP_OUTPUT_FORMAT\",\n \
                            \"value\": \"DY, DD MON YYYY HH24:MI:SS TZHTZM\"\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMESTAMP_TZ_OUTPUT_FORMAT\",\n \
                            \"value\": \"\"\n \
                          },\n \
                          {\n \
                            \"name\": \"CLIENT_RESULT_PREFETCH_SLOTS\",\n \
                            \"value\": 2\n \
                          }\n \
                        ],\n \
                      \"rowtype\":\n \
                        [\n \
                          {\n \
                            \"name\": \"select 1\",\n \
                            \"byteLength\": null,\n \
                            \"length\": null,\n \
                            \"type\": \"fixed\",\n \
                            \"nullable\": false,\n \
                            \"precision\": 1,\n \
                            \"scale\": 0\n \
                          }\n \
                        ],\n \
                      \"rowset\": [[\"1\"]],\n \
                      \"total\": 1,\n \
                      \"returned\": 1,\n \
                      \"queryId\": \"df2852ef-e082-4bb3-94a4-e540bf0e70c6\",\n \
                      \"databaseProvider\": null,\n \
                      \"finalDatabaseName\": null,\n \
                      \"finalSchemaName\": null,\n \
                      \"finalWarehouseName\": \"NEW_WH\",\n \
                      \"finalRoleName\": \"ACCOUNTADMIN\",\n \
                      \"numberOfBinds\": 0,\n \
                      \"statementTypeId\": 4096,\n \
                      \"version\": 0\n \
                    },\n \
                  \"message\": null,\n \
                  \"code\": null,\n \
                  \"success\": true\n \
                }"

// Session Gone Response
#define MOCK_RESPONSE_SESSION_GONE "{\n \
                  \"code\": \"390111\",\n \
                  \"data\": null,\n \
                  \"message\": \"ERROR!\",\n \
                  \"success\": false\n \
                }"

// Service name login
#define MOCK_URL_SERVICE_NAME_LOGIN "https://servicename.snowflakecomputing.com:443/session/v1/login-request"
#define MOCK_BODY_SERVICE_NAME_LOGIN "{\n\t\"data\":\t{\n\t\t\"CLIENT_APP_ID\":\t\"C API\",\n\t\t\"CLIENT_APP_VERSION\":\t\"0.0.0\",\n\t\t\"ACCOUNT_NAME\":\t\"servicename\",\n\t\t\"LOGIN_NAME\":\t\"servicenameuser\",\n\t\t\"PASSWORD\":\t\"secret-password\",\n\t\t\"CLIENT_ENVIRONMENT\":\t{\n\t\t\t\"APPLICATION\":\t\"C API\",\n\t\t\t\"OS\":\t\"Linux\",\n\t\t\t\"OS_VERSION\":\t\"0\"\n\t\t},\n\t\t\"SESSION_PARAMETERS\":\t{\n\t\t\t\"AUTOCOMMIT\":\t\"TRUE\",\n\t\t\t\"TIMEZONE\":\t\"UTC\"\n\t\t}\n\t}\n}"
#define MOCK_RESPONSE_SERVICE_NAME_LOGIN "{\n \
                  \"code\": null,\n \
                  \"data\":\n \
                    {\n \
                      \"displayUserName\": \"FAKEUSERNAME\",\n \
                      \"firstLogin\": false,\n \
                      \"healthCheckInterval\": 45,\n \
                      \"masterToken\": \"ETMsDgAAAVHApAq4ABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEFXybqPxPmJrBv1S2+TBVY0AAACgxIkcbg5ZTa3xHwhdUQ1kBjEbV1YJcYuG0ITQ8/jH37FhSO4UYHJCZ/TV0+CF5TBA+a4nkmyh1INKk6ndi7vWp1hZGM7rUtRkZMmdjnUTPKBZpXd8LFiGfofEus8K4OPEmf3PPvwrxMvGbzxNCTAHo/hiD8sVNkSjVXfPPJKwO7HHvnPuNP4LzpOf0mqqIWDHPCHYZjlsUOLM+f2CtKCgIgAUvY30vA343bf1eXUNqlCvH45kEVU=\",\n \
                      \"masterValidityInSeconds\": 14400,\n \
                      \"newClientForUpgrade\": null,\n \
                      \"remMeToken\": \"ETMsDgAAAVHApAq4ABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEFXybqPxPmJrBv1S2+TBVY0AAACgxIkcbg5ZTa3xHwhdUQ1kBjEbV1YJcYuG0ITQ8/jH37FhSO4UYHJCZ/TV0+CF5TBA+a4nkmyh1INKk6ndi7vWp1hZGM7rUtRkZMmdjnUTPKBZpXd8LFiGfofEus8K4OPEmf3PPvwrxMvGbzxNCTAHo/hiD8sVNkSjVXfPPJKwO7HHvnPuNP4LzpOf0mqqIWDHPCHYZjlsUOLM+f2CtKCgIgAUvY30vA343bf1eXUNqlCvH45kEVU=\",\n \
                      \"remMeValidityInSeconds\": 14400,\n \
                      \"serverVersion\": \"Dev\",\n \
                      \"sessionId\": \"51539800306\",\n \
                      \"token\": \"ETMsDgAAAVHApArOABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEH6XI1dGt/WBW5OrD4pFJF0AAABwUr+kbG6lEvgdJ1unUV69p3ZQWhMXh13TrFu2OXIA19TnaNFoFe7L0GOUASskpvLxBwNjRITjaO3cDBZnZ5Kthv7Vu6wPXwPp9vtXtWKzy+B58arDdXoF7ktIjNhGZWnGlvrEM0IOUBXVmO5jHdKUZwAU1T3TJfuZgjJNs2gKQtbm2+AW7Yg=\",\n \
                      \"validityInSeconds\": 3600,\n \
                      \"parameters\": [{\n \
                        \"name\": \"TIMEZONE\",\n \
                        \"value\": \"America/Los_Angeles\"\n \
                      }, {\n \
                        \"name\": \"TIMESTAMP_OUTPUT_FORMAT\",\n \
                        \"value\": \"DY, DD MON YYYY HH24:MI:SS TZHTZM\"\n \
                      }, {\n \
                        \"name\": \"TIMESTAMP_NTZ_OUTPUT_FORMAT\",\n \
                        \"value\": \"\"\n \
                      }, {\n \
                        \"name\": \"TIMESTAMP_LTZ_OUTPUT_FORMAT\",\n \
                        \"value\": \"\"\n \
                      }, {\n \
                        \"name\": \"TIMESTAMP_TZ_OUTPUT_FORMAT\",\n \
                        \"value\": \"\"\n \
                      }, {\n \
                        \"name\": \"DATE_OUTPUT_FORMAT\",\n \
                        \"value\": \"YYYY-MM-DD\"\n \
                      }, {\n \
                        \"name\": \"TIME_OUTPUT_FORMAT\",\n \
                        \"value\": \"HH24:MI:SS\"\n \
                      }, {\n \
                        \"name\": \"CLIENT_RESULT_PREFETCH_SLOTS\",\n \
                        \"value\": 2\n \
                      }, {\n \
                        \"name\": \"CLIENT_RESULT_PREFETCH_THREADS\",\n \
                        \"value\": 1\n \
                      }, {\n \
                        \"name\": \"CLIENT_HONOR_CLIENT_TZ_FOR_TIMESTAMP_NTZ\",\n \
                        \"value\": true\n \
                      }, {\n \
                        \"name\": \"CLIENT_USE_V1_QUERY_API\",\n \
                        \"value\": true\n \
                      }, {\n \
                        \"name\": \"CLIENT_DISABLE_INCIDENTS\",\n \
                        \"value\": true\n \
                      }, {\n \
                        \"name\": \"SERVICE_NAME\",\n \
                        \"value\": \"fakeservicename\"\n \
                      }, {\n \
                        \"name\": \"CLIENT_SESSION_KEEP_ALIVE\",\n \
                        \"value\": false\n \
                      }, {\n \
                        \"name\": \"CLIENT_SESSION_KEEP_ALIVE_HEARTBEAT_FREQUENCY\",\n \
                        \"value\": 3600\n \
                      }]\n \
                    },\n \
                  \"message\": null,\n \
                  \"success\": true\n \
                }"

// Service name query
#define MOCK_URL_SERVICE_NAME_QUERY "https://servicename.snowflakecomputing.com:443/queries/v1/query-request"
#define MOCK_RESPONSE_SERVICE_NAME_QUERY "{\n \
                  \"data\":\n \
                    {\n \
                      \"parameters\":\n \
                        [\n \
                          {\n \
                            \"name\": \"DATE_OUTPUT_FORMAT\",\n \
                            \"value\": \"YYYY-MM-DD\"\n \
                          },\n \
                          {\n \
                            \"name\": \"CLIENT_USE_V1_QUERY_API\",\n \
                            \"value\": true\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMESTAMP_LTZ_OUTPUT_FORMAT\",\n \
                            \"value\": \"\"\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMESTAMP_NTZ_OUTPUT_FORMAT\",\n \
                            \"value\": \"\"\n \
                          },\n \
                          {\n \
                            \"name\": \"CLIENT_RESULT_PREFETCH_THREADS\",\n \
                            \"value\": 1\n \
                          },\n \
                          {\n \
                            \"name\": \"CLIENT_HONOR_CLIENT_TZ_FOR_TIMESTAMP_NTZ\",\n \
                            \"value\": true\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMEZONE\",\n \
                            \"value\": \"America/Los_Angeles\"\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMESTAMP_OUTPUT_FORMAT\",\n \
                            \"value\": \"DY, DD MON YYYY HH24:MI:SS TZHTZM\"\n \
                          },\n \
                          {\n \
                            \"name\": \"TIMESTAMP_TZ_OUTPUT_FORMAT\",\n \
                            \"value\": \"\"\n \
                          },\n \
                          {\n \
                            \"name\": \"CLIENT_RESULT_PREFETCH_SLOTS\",\n \
                            \"value\": 2\n \
                          }\n \
                          ,\n \
                          {\n \
                            \"name\": \"SERVICE_NAME\",\n \
                            \"value\": \"fakeservicename\"\n \
                          }\n \
                        ],\n \
                      \"rowtype\":\n \
                        [\n \
                          {\n \
                            \"name\": \"c2\",\n \
                            \"byteLength\": null,\n \
                            \"length\": null,\n \
                            \"type\": \"fixed\",\n \
                            \"nullable\": false,\n \
                            \"precision\": 1,\n \
                            \"scale\": 0\n \
                          }\n \
                        ],\n \
                      \"rowset\": [[\"1\"]],\n \
                      \"total\": 1,\n \
                      \"returned\": 1,\n \
                      \"queryId\": \"df2852ef-e082-4bb3-94a4-e540bf0e70c6\",\n \
                      \"databaseProvider\": null,\n \
                      \"finalDatabaseName\": null,\n \
                      \"finalSchemaName\": null,\n \
                      \"finalWarehouseName\": \"NEW_WH\",\n \
                      \"finalRoleName\": \"ACCOUNTADMIN\",\n \
                      \"numberOfBinds\": 0,\n \
                      \"statementTypeId\": 4096,\n \
                      \"version\": 0\n \
                    },\n \
                  \"message\": null,\n \
                  \"code\": null,\n \
                  \"success\": true\n \
                }"

#define MOCK_URL_DELETE_CONNECTION_SERVICE_NAME "https://servicename.snowflakecomputing.com:443/session"

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKECLIENT_MOCK_ENDPOINTS_H
