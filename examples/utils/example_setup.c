#include <snowflake/logger.h>
#include "example_setup.h"

void initialize_snowflake_example(sf_bool debug) {
    // default location and the maximum logging
    snowflake_global_init(NULL, SF_LOG_TRACE);

    snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE"));
    snowflake_global_set_attribute(SF_GLOBAL_DEBUG, &debug);
}

SF_CONNECT *setup_snowflake_connection() {
    return setup_snowflake_connection_with_autocommit(
      "UTC", SF_BOOLEAN_TRUE);
}

void set_authentication_attributes(SF_CONNECT *sf) {
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));

    const char *priv_key_file = getenv("SNOWFLAKE_TEST_PRIVATE_KEY_FILE");
    if (priv_key_file && priv_key_file[0] != '\0') {
        snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR,
                                SF_AUTHENTICATOR_JWT);
        snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE, priv_key_file);
        const char *priv_key_pwd = getenv("SNOWFLAKE_TEST_PRIVATE_KEY_PWD");
        if (priv_key_pwd && priv_key_pwd[0] != '\0') {
            snowflake_set_attribute(sf, SF_CON_PRIV_KEY_FILE_PWD,
                                    priv_key_pwd);
        }
    } else {
        snowflake_set_attribute(sf, SF_CON_PASSWORD,
                                getenv("SNOWFLAKE_TEST_PASSWORD"));
    }
}

SF_CONNECT *setup_snowflake_connection_with_autocommit(
  const char* timezone, sf_bool autocommit) {
    SF_CONNECT *sf = snowflake_init();

    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    set_authentication_attributes(sf);
    snowflake_set_attribute(sf, SF_CON_DATABASE,
                            getenv("SNOWFLAKE_TEST_DATABASE"));
    snowflake_set_attribute(sf, SF_CON_SCHEMA, getenv("SNOWFLAKE_TEST_SCHEMA"));
    snowflake_set_attribute(sf, SF_CON_ROLE, getenv("SNOWFLAKE_TEST_ROLE"));
    snowflake_set_attribute(sf, SF_CON_WAREHOUSE,
                            getenv("SNOWFLAKE_TEST_WAREHOUSE"));
    snowflake_set_attribute(sf, SF_CON_AUTOCOMMIT, &autocommit);
    snowflake_set_attribute(sf, SF_CON_TIMEZONE, timezone);
    char *host, *port, *protocol;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host) {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port) {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol) {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }
    return sf;
}
