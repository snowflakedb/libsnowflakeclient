/*
 * Copyright (c) 2022 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <regex>
#include <functional>

#include "Authenticator.hpp"
#include "../logger/SFLogger.hpp"
#include "error.h"
#include "../cpp/entities.hpp"

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <connection.h>
#include "curl_desc_pool.h"

#include <fstream>

#include "snowflake/SF_CRTFunctionSafe.h"

#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef _WIN32
#include <Shellapi.h>
#define strcasecmp _stricmp
#endif

#define JWT_THROW(msg)                              \
{                                                   \
  throw Snowflake::Client::Jwt::JwtException(msg);  \
}

// wrapper functions for C
extern "C" {

  AuthenticatorType getAuthenticatorType(const char* authenticator)
  {
    if ((!authenticator) || (strlen(authenticator) == 0) ||
        (strcasecmp(authenticator, SF_AUTHENTICATOR_DEFAULT) == 0))
    {
      return AUTH_SNOWFLAKE;
    }
    if (strcasecmp(authenticator, SF_AUTHENTICATOR_JWT) == 0)
    {
      return AUTH_JWT;
    }
 
    return AUTH_OKTA;
    //return AUTH_UNSUPPORTED;
  }

  SF_STATUS STDCALL auth_initialize(SF_CONNECT * conn)
  {
    if (!conn)
    {
      return SF_STATUS_ERROR_NULL_POINTER;
    }

    AuthenticatorType auth_type = getAuthenticatorType(conn->authenticator);
    try
    {
      if (AUTH_JWT == auth_type)
      {
        conn->auth_object = static_cast<Snowflake::Client::IAuthenticator*>(
                              new Snowflake::Client::AuthenticatorJWT(conn));
      }
      if (AUTH_OKTA == auth_type)
      {
          conn->auth_object = static_cast<Snowflake::Client::IAuthenticator*>(
              new Snowflake::Client::AuthenticatorOKTA(conn));
      }
    }
    catch (...)
    {
      SET_SNOWFLAKE_ERROR(&conn->error, SF_STATUS_ERROR_GENERAL,
                          "authenticator initialization failed",
                          SF_SQLSTATE_GENERAL_ERROR);
      return SF_STATUS_ERROR_GENERAL;
    }

    return SF_STATUS_SUCCESS;
  }

  int64 auth_get_renew_timeout(SF_CONNECT * conn)
  {
    if (!conn || !conn->auth_object)
    {
      return 0;
    }

    try
    {
      return static_cast<Snowflake::Client::IAuthenticator*>(conn->auth_object)
               ->getAuthRenewTimeout();
    }
    catch (...)
    {
      return 0;
    }
  }

  SF_STATUS STDCALL auth_authenticate(SF_CONNECT * conn)
  {
    if (!conn || !conn->auth_object)
    {
      return SF_STATUS_SUCCESS;
    }

    try
    {
      static_cast<Snowflake::Client::IAuthenticator*>(conn->auth_object)->authenticate();
    }
    catch (...)
    {
      SET_SNOWFLAKE_ERROR(&conn->error, SF_STATUS_ERROR_GENERAL,
                          "authentication failed",
                          SF_SQLSTATE_GENERAL_ERROR);
      return SF_STATUS_ERROR_GENERAL;
    }

    return SF_STATUS_SUCCESS;
  }

  void auth_update_json_body(SF_CONNECT * conn, cJSON* body)
  {
     cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");
     if (!data)
     {
          data = snowflake_cJSON_CreateObject();
          snowflake_cJSON_AddItemToObject(body, "data", data);
     }
    if (!conn || !conn->auth_object)
    {
      return;
    }

    try
    {
      static_cast<Snowflake::Client::IAuthenticator*>(conn->auth_object)->
        updateDataMap(body);
    }
    catch (...)
    {
      ; // Do nothing
    }

    return;
  }

  void auth_renew_json_body(SF_CONNECT * conn, cJSON* body)
  {
    if (!conn || !conn->auth_object)
    {
      return;
    }

    try
    {
      static_cast<Snowflake::Client::IAuthenticator*>(conn->auth_object)->
        renewDataMap(body);
    }
    catch (...)
    {
      ; // Do nothing
    }

    return;
  }

  void STDCALL auth_terminate(SF_CONNECT * conn)
  {
    if (!conn || !conn->auth_object)
    {
      return;
    }

    AuthenticatorType auth_type = getAuthenticatorType(conn->authenticator);
    try
    {
      if (AUTH_JWT == auth_type)
      {
        delete static_cast<Snowflake::Client::AuthenticatorJWT*>(conn->auth_object);
      }
      if (AUTH_OKTA == auth_type)
      {
          delete static_cast<Snowflake::Client::AuthenticatorOKTA*>(conn->auth_object);
      }
    }
    catch (...)
    {
      ; // Do nothing
    }

    return;
  }

} // extern "C"

namespace Snowflake
{
namespace Client
{

  void IAuthenticator::renewDataMap(cJSON *dataMap)
  {
    authenticate();
    updateDataMap(dataMap);
  }

  void AuthenticatorJWT::loadPrivateKey(const std::string &privateKeyFile,
                                        const std::string &passcode)
  {
    FILE *file = nullptr;
    if (sf_fopen(&file, privateKeyFile.c_str(), "r") == nullptr)
    {
      CXX_LOG_ERROR("Failed to open private key file. Errno: %d", errno);
      JWT_THROW("Failed to open private key file");
    }

    m_privKey = PEM_read_PrivateKey(file, nullptr, nullptr, (void *)passcode.c_str());
    fclose(file);

    if (m_privKey == nullptr) {
      CXX_LOG_ERROR("Loading private key from %s failed", privateKeyFile.c_str());
      JWT_THROW("Marshaling private key failed");
    }
  }

  AuthenticatorJWT::AuthenticatorJWT(SF_CONNECT *conn)
  : m_jwt{Jwt::buildIJwt()}
  {
    // Check private key is valid
    std::string privKeyFile, privKeyFilePwd;
    if (conn && conn->priv_key_file)
    {
      privKeyFile = conn->priv_key_file;
    }
    if (conn && conn->priv_key_file_pwd)
    {
      privKeyFilePwd = conn->priv_key_file_pwd;
    }
    loadPrivateKey(privKeyFile, privKeyFilePwd);
    m_timeOut = conn->jwt_timeout;
    m_renewTimeout = conn->jwt_cnxn_wait_time;

    // Prepare header
    std::shared_ptr<Header> header {Header::buildHeader()};
    header->setAlgorithm(Snowflake::Client::Jwt::AlgorithmType::RS256);
    m_jwt->setHeader(std::move(header));

    // Prepare claim set
    std::shared_ptr<ClaimSet> claimSet {ClaimSet::buildClaimSet()};

    std::string account = conn->account;
    std::string user = conn->user;
    for (char &c : account) c = std::toupper(c);
    for (char &c : user) c = std::toupper(c);

    // issuer
    std::string subject = account + '.';
    subject += user;
    claimSet->addClaim("sub", subject);

    std::string issuer = subject + ".SHA256:" + extractPublicKey(m_privKey);
    claimSet->addClaim("iss", issuer);
    m_jwt->setClaimSet(std::move(claimSet));
  }

  AuthenticatorJWT::~AuthenticatorJWT()
  {
    if (m_privKey)
    {
      EVP_PKEY_free(m_privKey);
      m_privKey = NULL;
    }
  }

  void AuthenticatorJWT::authenticate()
  {
    using namespace std::chrono;
    const auto now = system_clock::now().time_since_epoch();
    const auto seconds = duration_cast<std::chrono::seconds>(now);
    auto claimSet = m_jwt->getClaimSet();
    claimSet->addClaim("iat", (long)seconds.count());
    claimSet->addClaim("exp", (long)seconds.count() + m_timeOut);
  }

  void AuthenticatorJWT::updateDataMap(cJSON* dataMap)
  {
    cJSON* data = snowflake_cJSON_GetObjectItem(dataMap, "data");
    snowflake_cJSON_DeleteItemFromObject(data, "AUTHENTICATOR");
    snowflake_cJSON_DeleteItemFromObject(data, "TOKEN");
    snowflake_cJSON_AddStringToObject(data, "AUTHENTICATOR", SF_AUTHENTICATOR_JWT);
    snowflake_cJSON_AddStringToObject(data, "TOKEN", m_jwt->serialize(m_privKey).c_str());
  }

  std::string AuthenticatorJWT::extractPublicKey(EVP_PKEY *privKey)
  {
    // extract the public key from the private key;
    unsigned char *out = nullptr;
    int size = i2d_PUBKEY(privKey, &out);
    if (size < 0)
    {
      CXX_LOG_ERROR("Fail to extract public key");
      JWT_THROW("Public Key extract failed");
    }
    std::vector<char> pubKeyBytes(out, out + size);
    OPENSSL_free(out);

    std::vector<char> sha256 = SHA256(pubKeyBytes);

    // Encoded into base64
    return Base64::encodePadding(sha256);
  }

  std::vector<char> AuthenticatorJWT::SHA256(const std::vector<char> &message)
  {
    std::unique_ptr<EVP_MD_CTX, std::function<void(EVP_MD_CTX *)>> mdctx
      {EVP_MD_CTX_new(), [](EVP_MD_CTX *ctx) {if (ctx) EVP_MD_CTX_free(ctx);}};

    if (mdctx == nullptr)
    {
      CXX_LOG_ERROR("EVP context create failed.");
      JWT_THROW("EVP context create failed");
    }

    if (1 != EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr))
    {
      CXX_LOG_ERROR("Digest Init failed.");
      JWT_THROW("Digest Init failed");
    }

    if (1 != EVP_DigestUpdate(mdctx.get(), message.data(), message.size()))
    {
      CXX_LOG_ERROR("Digest update failed.");
      JWT_THROW("Digest update failed");
    }

    std::vector<char> coded(EVP_MD_size(EVP_sha256()));
    unsigned int code_size;

    if (1 != EVP_DigestFinal_ex(mdctx.get(), (unsigned char *)coded.data(), &code_size))
    {
      CXX_LOG_ERROR("Digest final failed.");
      JWT_THROW("Digest final failed");
    }

    coded.resize(code_size);

    return coded;
  }

  AuthenticatorOKTA::AuthenticatorOKTA(
      SF_CONNECT* connection) : m_connection(connection)
  {
      // nop
  }

  AuthenticatorOKTA::~AuthenticatorOKTA()
  {
      // nop
  }

  void AuthenticatorOKTA::authenticate()
  {
      // 1. get authenticator info
      cJSON* respData = getIdpInfo();

      char* tokenURLStr = snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(respData, "tokenUrl"));
      char* ssoURLStr = snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(respData, "ssoUrl"));


      // 2. verify ssoUrl and tokenUrl contains same prefix
      if (!SFURL::urlHasSamePrefix(tokenURLStr, m_connection->authenticator))
      {
          CXX_LOG_ERROR("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
              "The specified authenticator is not supported, "
              "authenticator=%s, token url=%s, sso url=%s",
              m_connection->authenticator, tokenURLStr, ssoURLStr);
          //SF_THROWGEN3_NO_INCIDENT("SFAuthenticatorVerificationFailed",
          //    authenticatorStr.c_str(),
          //    "***", "***");
      }

      void* curl_desc;
      CURL* curl;
      curl_desc = get_curl_desc_from_pool(tokenURLStr, m_connection->proxy, m_connection->no_proxy);
      curl = get_curl_from_desc(curl_desc);

      // When renewTimeout < 0, postJson() throws RenewTimeoutException
      // for each retry attempt so we can renew the one time token
      unsigned retryTimeout = get_login_timeout(m_connection);
      using namespace std::chrono;
      const auto start = system_clock::now();
      unsigned long elapsedSeconds = 0;
      unsigned retriedCount = get_login_retry_count(m_connection);

      // 3. get one time token from okta
      cJSON* body = snowflake_cJSON_CreateObject();
      snowflake_cJSON_AddStringToObject(body, "username", m_connection->user);
      snowflake_cJSON_AddStringToObject(body, "password", m_connection->password);

      char* s_body = snowflake_cJSON_Print(body);

      // headers for step 4
      // add header for accept format
      SF_HEADER* header = sf_header_create();
      header->use_application_json_accept_type = SF_BOOLEAN_TRUE;
      if (create_header(m_connection, header, &m_connection->error)) {
          printf("Error");
      }
      cJSON* resp = NULL;

      if (curl_post_call(m_connection, curl, tokenURLStr, header, s_body, &resp,
          &m_connection->error, 0, get_login_retry_count(m_connection), retryTimeout,
          0, 0, false,
          false))
      {
          printf("%s", snowflake_cJSON_Print(resp));
          char* one_time_token = snowflake_cJSON_HasObjectItem(resp, "sessionToken") ?
              snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(resp, "sessionToken")) :
              snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(resp, "cookieToken"));
          auto end = std::chrono::system_clock::now();
          elapsedSeconds = static_cast<long>(duration_cast<std::chrono::seconds>(end - start).count());
          if (elapsedSeconds >= retryTimeout)
          {
              CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                  "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
                  retryTimeout, elapsedSeconds);

              //SF_THROWGEN1("OktaConnectionFailed", "timeout reached");
          }

          // 4. get SAML response
          SFURL sso_url = SFURL::parse(ssoURLStr);
          sso_url.addQueryParam("onetimetoken", one_time_token);
          resp = NULL;
          if (curl_get_call(m_connection, curl, (char*)sso_url.toString().c_str(), NULL, &resp, &m_connection->error))
          {
              elapsedSeconds = static_cast<long>(duration_cast<std::chrono::seconds>(end - start).count());
              if (elapsedSeconds >= retryTimeout)
              {
                  CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                      "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
                      retryTimeout, elapsedSeconds);

                  /*  SF_THROWGEN1("OktaConnectionFailed", "timeout reached");*/
              }
              m_samlResponse = snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(resp, "data"));
              printf("\n%s", m_samlResponse);
          }
          else
          {
              auto end = std::chrono::system_clock::now();
              elapsedSeconds = static_cast<long>(duration_cast<std::chrono::seconds>(end - start).count());
              if (elapsedSeconds >= retryTimeout)
              {
                  CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                      "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
                      retryTimeout, elapsedSeconds);

                  //SF_THROWGEN1("OktaConnectionFailed", "timeout reached");
              }

              CXX_LOG_TRACE("sf", "Connection", "Connect",
                  "Retry on getting SAML response with one time token renewed for %d times "
                  "with updated retryTimeout = %d",
                  retriedCount, retryTimeout - elapsedSeconds);
          }
          // 5. Validate post_back_url matches Snowflake URL
          std::string post_back_url = extractPostBackUrlFromSamlResponse(m_samlResponse);
          std::string server_url = getServerURLSync().toString();

            if ((!m_connection->disable_saml_url_check) &&
                (!SFURL::urlHasSamePrefix(post_back_url, server_url)))
            {
                CXX_LOG_ERROR("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                    "The specified authenticator and destination URL in "
                    "Saml Assertion did not "
                    "match, expected=%s, post back=%s",
                    server_url.c_str(),
                    post_back_url.c_str());
                    //SF_THROWGEN2_LOG_EXCEPTION("SFSamlResponseVerificationFailed",
                    //    "***",
                    //    "***",
                    //    m_connection);
             }

      }
      else
      {
                  CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                      "Fail to get SAML response, response body=%s",
                      snowflake_cJSON_Print(resp));

          //        SF_THROWGEN1("OktaConnectionFailed", std::to_string(resp.code()));


      }
  }

  void AuthenticatorOKTA::updateDataMap(cJSON* dataMap)
  {
      cJSON* data = snowflake_cJSON_GetObjectItem(dataMap, "data");
      snowflake_cJSON_DeleteItemFromObject(data, "AUTHENTICATOR");
      snowflake_cJSON_DeleteItemFromObject(data, "TOKEN");
      snowflake_cJSON_DeleteItemFromObject(data, "LOGIN_NAME");
      snowflake_cJSON_DeleteItemFromObject(data, "PASSWORD");
      snowflake_cJSON_DeleteItemFromObject(data, "EXT_AUTHN_DUO_METHOD");

      snowflake_cJSON_AddStringToObject(data, "RAW_SAML_RESPONSE", m_samlResponse.c_str());
  }

  std::string AuthenticatorOKTA::extractPostBackUrlFromSamlResponse(std::string html)
  {
      std::size_t form_start = html.find("<form");

      std::size_t post_back_start = html.find("action=\"", form_start);
     post_back_start += 8;
      std::size_t post_back_end = html.find("\"", post_back_start);

     std::string post_back_url = html.substr(post_back_start,
          post_back_end - post_back_start);
      CXX_LOG_TRACE("sf", "AuthenticatorOKTA",
          "extractPostBackUrlFromSamlResponse",
          "Post back url before unescape: %s", post_back_url.c_str());
      char unescaped_url[200];
      decode_html_entities_utf8(unescaped_url, post_back_url.c_str());
      CXX_LOG_TRACE("sf", "AuthenticatorOKTA",
          "extractPostBackUrlFromSamlResponse",
          "Post back url after unescape: %s", unescaped_url);
      return std::string(unescaped_url);
  }

  cJSON* AuthenticatorOKTA::getIdpInfo()
  {
      cJSON* dataMap = snowflake_cJSON_CreateObject();
      // connection URL
     const char* connectURL = "/session/authenticator-request";

      // login info as a json post body
     snowflake_cJSON_AddStringToObject(dataMap, "CLIENT_APP_ID", m_connection->application);
     snowflake_cJSON_AddStringToObject(dataMap, "CLIENT_APP_VERSION", m_connection->application_version);
      snowflake_cJSON_AddStringToObject(dataMap, "ACCOUNT_NAME",m_connection->account);
      snowflake_cJSON_AddStringToObject(dataMap, "AUTHENTICATOR",m_connection->authenticator);
      snowflake_cJSON_AddStringToObject(dataMap, "LOGIN_NAME",m_connection->user);
      snowflake_cJSON_AddStringToObject(dataMap, "PORT", m_connection->port);
      snowflake_cJSON_AddStringToObject(dataMap, "PROTOCOL", m_connection->protocol);

      cJSON* authnData = snowflake_cJSON_CreateObject();
      cJSON *resp = NULL;
      snowflake_cJSON_AddItemReferenceToObject(authnData, "data", dataMap);

      // add headers for account and authentication
      SF_HEADER* httpExtraHeaders = sf_header_create();
      if (!create_header(m_connection, httpExtraHeaders, &m_connection->error)) {
          printf("failed");
      }

      bool injectCURLTimeout = false;
      unsigned renew_timeout = 0;
      unsigned retriedCount = 0;
      sf_bool isNewRetry = true;

      char* s_body = snowflake_cJSON_Print(authnData);

      if (request(m_connection, &resp, connectURL, NULL,
          0, s_body, httpExtraHeaders,
          POST_REQUEST_TYPE, &m_connection->error, SF_BOOLEAN_FALSE,
          renew_timeout, get_login_retry_count(m_connection), get_login_timeout(m_connection), NULL,
          NULL, NULL, SF_BOOLEAN_TRUE)) 
      {
          printf("%s", snowflake_cJSON_Print(resp));
          return snowflake_cJSON_GetObjectItem(resp, "data");
      }
      else {
          cJSON* result = snowflake_cJSON_GetObjectItem(resp, "data");
          CXX_LOG_INFO("sf", "Connection", "getIdpInfo",
            "Fail to get authenticator info, response body=%s\n",
              snowflake_cJSON_Print(result));
            //SF_THROWGEN1("SFConnectionFailed", snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(result, "code")));
      }
  }

  SFURL AuthenticatorOKTA::getServerURLSync()
  {
      SFURL url = SFURL().scheme(m_connection->protocol)
          .host(m_connection->host)
          .port(m_connection->port);
      if (!m_connection->proxy_with_env)
      {
          // Set the proxy through curl option which won't affect other connections.
          url.setProxy(m_proxySettings);
      }
      return url;
  }

} // namespace Client
} // namespace Snowflake
