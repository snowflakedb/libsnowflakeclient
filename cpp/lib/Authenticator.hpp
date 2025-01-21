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
#include "snowflake/SFURL.hpp"
#include "../../lib/snowflake_util.h"
#include "../include/snowflake/IAuth.hpp"
#include "picojson.h"

namespace Snowflake
{
namespace Client
{
    using namespace Snowflake::Client::IAuth;
  /**
   * JWT Authenticator
   */
  class AuthenticatorJWT : public IAuthenticator
  {
  public:
    AuthenticatorJWT(SF_CONNECT *conn);

    ~AuthenticatorJWT();

    void authenticate();

    void updateDataMap(jsonObject_t& dataMap);

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

  class CIDPAuthenticator : public IDPAuthenticator
  {
  public:
      CIDPAuthenticator(SF_CONNECT* conn);
      ~CIDPAuthenticator();

      // If the function fails, ensure to define and return an appropriate error message at m_errMsg.
      bool curlPostCall(SFURL& url, const jsonObject_t& body, jsonObject_t& resp);
      bool curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& raw_data, bool& isRetry);
  private:
      SF_CONNECT* m_connection;
  };

  class AuthenticatorOKTA : public IAuthenticatorOKTA
  {
  public:
      AuthenticatorOKTA(SF_CONNECT *conn);

      ~AuthenticatorOKTA();

      void authenticate();

      void updateDataMap(jsonObject_t& dataMap);

  private:
      SF_CONNECT* m_connection;
  };

  class AuthenticatorExternalBrowser : public IAuthenticatorExternalBrowser
  {
  public:
      AuthenticatorExternalBrowser(
          SF_CONNECT* connection, IAuthWebServer* authWebServer = nullptr);

      virtual ~AuthenticatorExternalBrowser();

      void authenticate();
  private:
      typedef Snowflake::Client::Util::IBase64 Base64;
      SF_CONNECT* m_connection;
  };
} // namespace Client
} // namespace Snowflake
#endif //PROJECT_AUTHENTICATOR_HPP
