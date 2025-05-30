#ifndef SNOWFLAKE_EXAMPLE_H
#define SNOWFLAKE_EXAMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_WIN32)
#define STDCALL
#else
#define STDCALL __stdcall
#endif

#include <snowflake/client.h>

void initialize_snowflake_example(sf_bool debug);
SF_CONNECT *setup_snowflake_connection();
SF_CONNECT *setup_snowflake_connection_with_autocommit(
  const char* timezone, sf_bool autocommit);
#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_EXAMPLE_H
