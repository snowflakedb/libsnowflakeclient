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
#include "../include/snowflake/IAuth.hpp"

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

  class AuthenticatorOKTA : public IAuthenticatorOKTA
  {
  public:
      AuthenticatorOKTA(SF_CONNECT *conn);

      ~AuthenticatorOKTA();

      void authenticate();

      void updateDataMap(jsonObject_t& dataMap);
      
      // If the function fails, ensure to define and return an appropriate error message at m_errMsg.
      bool curlPostCall(SFURL& url, const jsonObject_t& body, jsonObject_t& resp);

      bool curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& raw_data, bool& isRetry);

  private:
      SF_CONNECT* m_connection;
  };

 /**
  * Web Server for external Browser authentication
  */
  class AuthWebServer : public IAuthWebServer
  {

  private:
      SOCKET m_socket_descriptor; // socket
      SOCKET m_socket_desc_web_client; // socket (client)
      int m_port; // port to listen
      std::string m_saml_token;
      bool m_consent_cache_id_token;
      std::string m_origin;
      int m_timeout;

      void parseAndRespondOptionsRequest(std::string response);
      void parseAndRespondPostRequest(std::string response);
      void parseAndRespondGetRequest(char** rest_mesg);
      void respond(std::string queryParameters);
      void respondJson(jsonValue_t& json);

      std::vector<std::string> splitString(const std::string& s, char delimiter);
      std::string unquote(std::string src);
      std::vector<std::pair<std::string, std::string>> splitQuery(std::string query);

  public:
      AuthWebServer();

      ~AuthWebServer();

      /**
       * Start a web server
       */
      void start();

      /**
       * Stop a web server
       */
      void stop();

      /**
       * Start accepting the request
       */
      void startAccept();

      /**
       * Receive a SAML token
       */
      bool receive();

      /**
       * Return the port listening to accept SAML token.
       * @return port number
       */
      int getPort();

      /**
       * Return SAML Token provided by GS.
       * @return SAML token
       */
      std::string getSAMLToken();

      /**
       * Did consent cache id token?
       * @return true or false
       */
      bool isConsentCacheIdToken();

      /**
       * Set the timeout for the web server.
       */
      void setTimeout(int timeout);
  };

  class AuthenticatorExternalBrowser : public IAuthenticator, public IDPAuthenticator
  {
  public:
      AuthenticatorExternalBrowser(
          SF_CONNECT* connection, IAuthWebServer* authWebServer = nullptr);

      virtual ~AuthenticatorExternalBrowser();

      int getPort(void);

      void authenticate();

      void updateDataMap(jsonObject_t& dataMap);

      /**
       * Start web browser so that the user can type IdP user and password
       * @param ssoUrl SSO URL
       */
      virtual void startWebBrowser(std::string ssoUrl);

      /**
       * Get Login URL for multiple SAML
       * @param out the login URL
       * @param port port number listening to get SAML token
       */
      virtual void getLoginUrl(std::map<std::string, std::string>& out, int port);

      /**
       * Generate the proof key
       * @return The proof key
       */
      virtual std::string generateProofKey();

  private:
      typedef Snowflake::Client::Util::IBase64 Base64;

      SF_CONNECT* m_connection;
      IAuthWebServer* m_authWebServer;
  protected:
      std::string m_proofKey;
      std::string m_token;
      bool m_consentCacheIdToken;
      std::string m_origin;

#ifdef __APPLE__
      void openURL(const std::string& url_str);
#endif

  };

#if defined(WIN32) || defined(_WIN64)
  /**
   * Winsock start and cleanup
   */
  class AuthWinSock
  {
  public:
      AuthWinSock();
      ~AuthWinSock();
  };
#endif


} // namespace Client
} // namespace Snowflake
#endif //PROJECT_AUTHENTICATOR_HPP
