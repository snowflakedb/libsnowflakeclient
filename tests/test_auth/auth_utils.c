#include <stdlib.h>
#include "auth_utils.h"

void set_all_snowflake_attributes(SF_CONNECT *sf) {
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_AUTH_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_AUTH_TEST_USER"));

    char *host, *port, *protocol, *role;
    host = getenv("SNOWFLAKE_AUTH_TEST_HOST");
    if (host) {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_AUTH_TEST_PORT");
    if (port) {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_AUTH_TEST_PROTOCOL");
    if (protocol) {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }
    role = getenv("SNOWFLAKE_AUTH_TEST_ROLE");
    if (role) {
        snowflake_set_attribute(sf, SF_CON_ROLE, role);
    }
}
