#ifndef SNOWFLAKE_AUTHENTICATOR_H
#define SNOWFLAKE_AUTHENTICATOR_H

#include "snowflake/client.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum authenticator_type
    {
        AUTH_SNOWFLAKE,
        AUTH_OAUTH,
        AUTH_OAUTH_AUTHORIZATION_CODE,
        AUTH_OAUTH_CLIENT_CREDENTIALS,
        AUTH_OKTA,
        AUTH_EXTERNALBROWSER,
        AUTH_ID_TOKEN,
        AUTH_JWT,
        AUTH_USR_PWD_MFA,
        AUTH_PAT,

        AUTH_TEST,
        AUTH_UNSUPPORTED
    } AuthenticatorType;

    /**
     * Retrieve authenticator type from authenticator setting in string.
     *
     * @param authenticator        The authenticator setting in string
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    AuthenticatorType getAuthenticatorType(const char* authenticator);

    /**
     * Initialize authenticator
     *
     * @param conn                 The connection
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL auth_initialize(SF_CONNECT * conn);

    /**
     * Retrieve renew timeout for authentication
     *
     * @param conn                 The connection
     *
     * @return The renew timeout for authentication in seconds if needed,
     *         0 means no renew timeout.
     */
    int64 auth_get_renew_timeout(SF_CONNECT * conn);

    /**
     * do autentication
     *
     * @param conn                 The connection
     *
     * @return 0 if successful, otherwise an error is returned.
     */
    SF_STATUS STDCALL auth_authenticate(SF_CONNECT * conn);

    /**
     * update autentication information in json body of the connection request
     *
     * @param conn                 The connection
     * @param body                 The json body for connection request
     */
    void auth_update_json_body(SF_CONNECT * conn, cJSON* body);

    /**
     * renew autentication information in json body when renew timeout reached
     *
     * @param conn                 The connection
     * @param body                 The json body for connection request
     */
    void auth_renew_json_body(SF_CONNECT * conn, cJSON* body);

    /**
    * Terminate authenticator
    *
    * @param conn                 The connection
    */
    void STDCALL auth_terminate(SF_CONNECT * conn);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SNOWFLAKE_AUTHENTICATOR_H
