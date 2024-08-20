/*
 * Copyright (c) 2022 Snowflake Computing, Inc. All rights reserved.
 */

#pragma once
#ifndef PROJECT_AUTHENTICATOR_HPP
#define PROJECT_AUTHENTICATOR_HPP

#include <atomic>
#include <string>
#include <map>

#include "snowflake/basic_types.h"
#include "snowflake/client.h"
#include "snowflake/IJwt.hpp"
#include "snowflake/IBase64.hpp"
#include "authenticator.h"
#include "cJSON.h"

namespace Snowflake
{
  namespace Client
  {
    /**
     * Authenticator
     */
    class IAuthenticator
    {
    public:
      IAuthenticator() : m_renewTimeout(0)
      {
      }

      virtual ~IAuthenticator()
      {
      }

      virtual void authenticate() = 0;

      virtual void updateDataMap(cJSON *dataMap) = 0;

      // Retrieve authenticator renew timeout, return 0 if not available.
      // When the authenticator renew timeout is available, the connection should
      // renew the authentication (call renewDataMap) for each time the
      // authenticator specific timeout exceeded within the entire login timeout.
      int64 getAuthRenewTimeout()
      {
        return m_renewTimeout;
      }

      // Renew the autentication and update datamap.
      // The default behavior is to call authenticate() and updateDataMap().
      virtual void renewDataMap(cJSON *dataMap);

    protected:
      int64 m_renewTimeout;
    };

    /**
     * JWT Authenticator
     */
    class AuthenticatorJWT : public IAuthenticator
    {
    public:
      AuthenticatorJWT(SF_CONNECT *conn);

      ~AuthenticatorJWT();

      void authenticate();

      void updateDataMap(cJSON *dataMap);

    private:
      void loadPrivateKey(const std::string &privateKeyFile, const std::string &passcode);

      typedef Snowflake::Client::Jwt::IJwt Jwt;
      typedef Snowflake::Client::Jwt::IClaimSet ClaimSet;
      typedef Snowflake::Client::Jwt::IHeader Header;
      typedef Snowflake::Client::Util::IBase64 Base64;
      typedef std::unique_ptr<Jwt> JwtPtr;

      EVP_PKEY *m_privKey;
      int64 m_timeOut;
      JwtPtr m_jwt;

      static std::string extractPublicKey(EVP_PKEY *privKey);

      static std::vector<char> SHA256(const std::vector<char> &message);
    };

    class AuthenticatorUserMFA : public IAuthenticator
    {
    public:
      AuthenticatorUserMFA(SF_CONNECT *conn);

      ~AuthenticatorUserMFA();

      void authenticate();

      void updateDataMap(cJSON *dataMap);

    private:
      std::string m_passcode;
      bool m_passcodeInPassword;
    };

  } // namespace Client
} // namespace Snowflake
#endif // PROJECT_AUTHENTICATOR_HPP
