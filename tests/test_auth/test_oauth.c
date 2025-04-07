#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "auth_utils.h"

char *getToken() {
    char *command = "node /externalbrowser/getOauthToken.js";
    char buffer[128];
    char *token = NULL;
    size_t token_len = 0;
    FILE *pipe = popen(command, "r");

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t buffer_len = strlen(buffer);
        char *new_token = realloc(token, token_len + buffer_len + 1);
        if (!new_token) {
            free(token);
            perror("realloc failed");
            pclose(pipe);
            return NULL;
        }
        token = new_token;
        strcpy(token + token_len, buffer);
        token_len += buffer_len;
    }

    if (token) {
        // Remove newline characters
        char *newline;
        while ((newline = strchr(token, '\n')) != NULL) {
            *newline = '\0';
        }
    }

    pclose(pipe);
    return token;
}

void test_oauth_successful_connection(void **unused) {
    SF_UNUSED(unused);

    SF_CONNECT * sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    char *token = getToken();
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "oauth");
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, token);

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
    free(token);
}

void test_oauth_invalid_token(void **unused) {
    SF_UNUSED(unused);

    SF_CONNECT * sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "oauth");
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, "invalidToken");

    SF_STATUS status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
    SF_ERROR_STRUCT* error = snowflake_error(sf);
    assert_string_equal(error->msg, "Invalid OAuth access token. ");

    snowflake_term(sf);
}

void test_oauth_mismatched_username(void **unused) {
    SF_UNUSED(unused);

    SF_CONNECT * sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    char *token = getToken();

    snowflake_set_attribute(sf, SF_CON_USER, "differentUsername");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "oauth");
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, token);

    SF_STATUS status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
    SF_ERROR_STRUCT* error = snowflake_error(sf);
    assert_string_equal(error->msg, "The user you were trying to authenticate as differs from the user tied to the access token.");

    snowflake_term(sf);
    free(token);
}
