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
#include "picojson.h"
#include "snowflake/SFURL.hpp"
#include "../../lib/snowflake_util.h"

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
    {}

    virtual ~IAuthenticator()
    {}

    virtual void authenticate()=0;

    virtual void updateDataMap(jsonObject_t& dataMap)=0;

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
    virtual void renewDataMap(jsonObject_t& dataMap);

  protected:
    int64 m_renewTimeout;
  };

  class IDPAuthenticator : public IAuthenticator
  {
  public:
      IDPAuthenticator(SF_CONNECT* conn) : m_connection(conn)
      {};

      ~IDPAuthenticator()
      {}

  protected:
     /*
      * Get IdpInfo for OKTA and SAML 2.0 application
      */
      void getIDPInfo();
      virtual void IDPPostCall(jsonObject_t& authData, jsonObject_t& resp);
      jsonObject_t respdata;
      SF_CONNECT* m_connection;
      const std::string connectURL = "/session/authenticator-request";
      std::string tokenURLStr;
      std::string ssoURLStr;
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

    void updateDataMap(jsonObject_t &dataMap);

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


  class AuthenticatorOKTA : public IDPAuthenticator
  {
  public:
      AuthenticatorOKTA(SF_CONNECT* conn);

      ~AuthenticatorOKTA();

      void authenticate();

      void updateDataMap(jsonObject_t& dataMap);

  protected:
      //Step3
      virtual void getOneTimeToken(jsonObject_t& dataMap);
      //Step4
      virtual void getSAMLResponse();


  private:
      std::string m_samlResponse;
      std::string oneTimeToken;

      /**
       * Extract post back url from samel response. Input is in HTML format.
      */
      std::string extractPostBackUrlFromSamlResponse(std::string html);
      SFURL getServerURLSync();
  };
} // namespace Client
} // namespace Snowflake
#endif //PROJECT_AUTHENTICATOR_HPP
