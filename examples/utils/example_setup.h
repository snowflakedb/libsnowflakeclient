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

/**
 * Configure user credentials on an SF_CONNECT based on environment variables.
 *
 * Uses keypair (JWT) authentication when SNOWFLAKE_TEST_PRIVATE_KEY_FILE is
 * defined, with an optional passphrase from SNOWFLAKE_TEST_PRIVATE_KEY_PWD.
 * Otherwise falls back to password authentication via SNOWFLAKE_TEST_PASSWORD.
 */
void set_authentication_attributes(SF_CONNECT *sf);
#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_EXAMPLE_H
