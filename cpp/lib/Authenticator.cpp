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
#include "../include/snowflake/entities.hpp"

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <connection.h>
#include "curl_desc_pool.h"
#include "snowflake/Exceptions.hpp"
#include "cJSON.h"
#include "../cpp/jwt/Util.hpp"

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

#define AUTH_THROW(msg)                                  \
{                                                         \
  throw Snowflake::Client::Exception::AuthException(msg);  \
}

#define JWT_THROW(err, msg)                                                                                                                 \
{                                                                                                                                           \
  SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_GENERAL, "Failed to created the header for the okta authentication", SF_SQLSTATE_GENERAL_ERROR); \
  AUTH_THROW(err);                                                                                                                          \
}

#define RETRY_THROW(elapsedSeconds, retriedCount)                                                                                                                 \
{                                                                                                                                           \
  throw Snowflake::Client::Exception::RenewTimeoutException(elapsedSeconds, retriedCount, false);                                                                                                 \
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
    if (strcasecmp(authenticator, SF_AUTHENTICATOR_OAUTH) == 0)
    {
        return AUTH_OAUTH;
    }

     return AUTH_OKTA;

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

    snowflake_cJSON_DeleteItemFromObject(data, "AUTHENTICATOR");
    snowflake_cJSON_DeleteItemFromObject(data, "TOKEN");
    if (AUTH_OAUTH == getAuthenticatorType(conn->authenticator)) 
    {
        snowflake_cJSON_AddStringToObject(data, "AUTHENTICATOR", SF_AUTHENTICATOR_OAUTH);
        snowflake_cJSON_AddStringToObject(data, "TOKEN", conn->oauth_token);
    }

    if (!conn || !conn->auth_object)
    {
      return;
    }

    try
    {
        jsonObject_t picoBody;
        cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");
        cJSONtoPicoJson(data, picoBody);
        static_cast<Snowflake::Client::IAuthenticator*>(conn->auth_object)->
            updateDataMap(picoBody);
        picoJsonTocJson(picoBody, &body);
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
        jsonObject_t picoBody;
        cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");
        cJSONtoPicoJson(data, picoBody);
        static_cast<Snowflake::Client::IAuthenticator*>(conn->auth_object)->
            renewDataMap(picoBody);
        picoJsonTocJson(picoBody, &body);
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
    using namespace picojson;

  void IAuthenticator::renewDataMap(jsonObject_t& dataMap)
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
      AUTH_THROW("Failed to open private key file");
    }

    m_privKey = PEM_read_PrivateKey(file, nullptr, nullptr, (void *)passcode.c_str());
    fclose(file);

    if (m_privKey == nullptr) {
      CXX_LOG_ERROR("Loading private key from %s failed", privateKeyFile.c_str());
      AUTH_THROW("Marshaling private key failed");
    }
  }

  void IdentityAuthenticator::getIDPInfo()
  {
      // login info as a json post body
      //Currently, the server is not enabled Okta authentication with C API. For testing purpose, I used the ODBC info.
      jsonObject_t dataMap;
      dataMap["ACCOUNT_NAME"] = value(m_connection->account);
      dataMap["AUTHENTICATOR"] = value(m_connection->authenticator);
      dataMap["LOGIN_NAME"] = value(m_connection->user);
      dataMap["PORT"] = value(m_connection->port);
      dataMap["PROTOCOL"] = value(m_connection->protocol);
      dataMap["CLIENT_APP_ID"] = value("ODBC");
      dataMap["CLIENT_APP_VERSION"] = value("3.4.1");

      jsonObject_t authnData, respData;
      authnData["data"] = value(dataMap);
      IDPPostCall(authnData, respData);
      tokenURLStr = respData["tokenUrl"].get<std::string>();
      ssoURLStr = respData["ssoUrl"].get<std::string>();
  }

  void IdentityAuthenticator::IDPPostCall(jsonObject_t& authData, jsonObject_t& respData)
  {
      // connection URL
      int64 elasped_time = 0;
      int64 renew_timeout = 0;
      int8* retried_count = &m_connection->retry_count;

      // add headers for account and authentication
      SF_ERROR_STRUCT* err = &m_connection->error;
      SF_HEADER* httpExtraHeaders = sf_header_create();
      std::string s_body = value(authData).serialize();

      httpExtraHeaders->use_application_json_accept_type = SF_BOOLEAN_TRUE;
      if (!create_header(m_connection, httpExtraHeaders, &m_connection->error)) {
          log_trace("sf", "AuthenticatorOKTA",
              "getIdpInfo",
              "Failed to create the header for the request to get the token URL and the SSO URL");
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_GENERAL, "OktaConnectionFailed: timeout reached", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW(err);
          goto cleanup;
      }

      cJSON* resp = NULL;
      if (!request(m_connection, &resp, connectURL.c_str(), NULL,
          0, (char*) s_body.c_str(), httpExtraHeaders,
          POST_REQUEST_TYPE, err, SF_BOOLEAN_FALSE,
          renew_timeout, get_login_retry_count(m_connection), get_login_timeout(m_connection), &elasped_time,
          retried_count, NULL, SF_BOOLEAN_TRUE))
      {
          log_info("sf", "Connection", "getIdpInfo",
              "Fail to get authenticator info, response body=%s\n",
              snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(resp, "data")));
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_GENERAL, "SFConnectionFailed", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW(err);
          goto cleanup;
      }

      //deduct elasped time from the retry timeout
      m_connection->retry_timeout -= elasped_time;
      authData.clear();
      cJSONtoPicoJson(snowflake_cJSON_GetObjectItem(resp, "data"), respData);
  cleanup:
      sf_header_destroy(httpExtraHeaders);
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
    try {
        loadPrivateKey(privKeyFile, privKeyFilePwd);
        m_timeOut = conn->jwt_timeout;
        m_renewTimeout = conn->jwt_cnxn_wait_time;

        // Prepare header
        std::shared_ptr<Header> header{ Header::buildHeader() };
        header->setAlgorithm(Snowflake::Client::Jwt::AlgorithmType::RS256);
        m_jwt->setHeader(std::move(header));

        // Prepare claim set
        std::shared_ptr<ClaimSet> claimSet{ ClaimSet::buildClaimSet() };

        std::string account = conn->account;
        std::string user = conn->user;
        for (char& c : account) c = std::toupper(c);
        for (char& c : user) c = std::toupper(c);

        // issuer
        std::string subject = account + '.';
        subject += user;
        claimSet->addClaim("sub", subject);

        std::string issuer = subject + ".SHA256:" + extractPublicKey(m_privKey);
        claimSet->addClaim("iss", issuer);
        m_jwt->setClaimSet(std::move(claimSet));
    }
    catch (Exception::AuthException e) {
        SET_SNOWFLAKE_ERROR(&conn->error, SF_STATUS_ERROR_GENERAL, e.message_.c_str(), SF_SQLSTATE_GENERAL_ERROR);
        AUTH_THROW("JWT Authentication failed");
    }
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

  void AuthenticatorJWT::updateDataMap(jsonObject_t &dataMap)
  {
    dataMap["AUTHENTICATOR"] = picojson::value(SF_AUTHENTICATOR_JWT);
    dataMap["TOKEN"] = picojson::value(m_jwt->serialize(m_privKey));
  }

  std::string AuthenticatorJWT::extractPublicKey(EVP_PKEY *privKey)
  {
    // extract the public key from the private key;
    unsigned char *out = nullptr;
    int size = i2d_PUBKEY(privKey, &out);
    if (size < 0)
    {
      CXX_LOG_ERROR("Fail to extract public key");
      AUTH_THROW("Public Key extract failed");
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
      AUTH_THROW("EVP context create failed");
    }

    if (1 != EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr))
    {
      CXX_LOG_ERROR("Digest Init failed.");
      AUTH_THROW("Digest Init failed");
    }

    if (1 != EVP_DigestUpdate(mdctx.get(), message.data(), message.size()))
    {
      CXX_LOG_ERROR("Digest update failed.");
      AUTH_THROW("Digest update failed");
    }

    std::vector<char> coded(EVP_MD_size(EVP_sha256()));
    unsigned int code_size;

    if (1 != EVP_DigestFinal_ex(mdctx.get(), (unsigned char *)coded.data(), &code_size))
    {
      CXX_LOG_ERROR("Digest final failed.");
      AUTH_THROW("Digest final failed");
    }

    coded.resize(code_size);

    return coded;
  }

  AuthenticatorOKTA::AuthenticatorOKTA(
      SF_CONNECT* connection) : IdentityAuthenticator(connection)
  {
      // nop
  }

  AuthenticatorOKTA::~AuthenticatorOKTA()
  {
      // nop
  }

  void AuthenticatorOKTA::getOneTimeToken(jsonObject_t& respData)
  {
      void* curl_desc;
      CURL* curl;
      curl_desc = get_curl_desc_from_pool(tokenURLStr.c_str(), m_connection->proxy, m_connection->no_proxy);
      curl = get_curl_from_desc(curl_desc);
      SF_ERROR_STRUCT* err = &m_connection->error;

      // When renewTimeout < 0, throws Exception
      // for each retry attempt so we can renew the one time token
      int64 retry_timeout = get_login_timeout(m_connection);
      int64 retry_max_count = get_login_retry_count(m_connection);

      int64 elapsed_time = 0;
      int8* retried_count = &m_connection->retry_count;

      // headers for step 4
      // add header for accept format
      SF_HEADER* header = sf_header_create();
      header->use_application_json_accept_type = SF_BOOLEAN_TRUE;
      if (!create_header(m_connection, header, &m_connection->error)) {
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_GENERAL, "Failed to created the header for the okta authentication", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW(err);
      }
      cJSON* resp = NULL;
      std::string s_body = picojson::value(respData).serialize();
          if (!curl_post_call(m_connection, curl, (char*)tokenURLStr.c_str(), header, (char*)s_body.c_str(), &resp,
              err, 0, retry_max_count, retry_timeout,
              &elapsed_time, retried_count, 0, false))
          {
              CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                  "Fail to get SAML response, response body=%s",
                  snowflake_cJSON_Print(resp));
              SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_BAD_REQUEST, "OktaConnectionFailed", SF_SQLSTATE_GENERAL_ERROR);
              goto cleanup;
          }
          if (elapsed_time >= retry_timeout)
          {
              CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                  "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
                  *retried_count, elapsed_time);
              SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_REQUEST_TIMEOUT, "OktaConnectionFailed: timeout reached", SF_SQLSTATE_GENERAL_ERROR);
              goto cleanup;
          }
      //update retry info.
      m_connection->retry_timeout -= elapsed_time;
      respData.clear();
      cJSONtoPicoJson(resp, respData);

  cleanup:
      sf_header_destroy(header);
      snowflake_cJSON_Delete(resp);
      free_curl_desc(curl_desc);
      if (err->error_code != SF_STATUS_SUCCESS) {
          AUTH_THROW(err);
      }
  }

  bool AuthenticatorOKTA::getSAMLResponse()
  {
      bool isRetry = false;
      int64 retry_timeout = get_login_timeout(m_connection);
      int64 retry_max_count = get_login_retry_count(m_connection);
      int64 elapsed_time = 0;
      int8 retried_count = m_connection->retry_count;

      void* curl_desc;
      CURL* curl;
      curl_desc = get_curl_desc_from_pool(ssoURLStr.c_str(), m_connection->proxy, m_connection->no_proxy);
      curl = get_curl_from_desc(curl_desc);
      SF_ERROR_STRUCT* err = &m_connection->error;
      SFURL sso_url = SFURL::parse(ssoURLStr);
      sso_url.addQueryParam("onetimetoken", oneTimeToken);
      cJSON* resp = NULL;
      if (!curl_get_call(m_connection, curl, (char*)sso_url.toString().c_str(), NULL, &resp, err, -1, retry_max_count, retry_timeout, &elapsed_time, &retried_count))
      {
          if (elapsed_time >= retry_timeout || retried_count >= retry_max_count)
          {
              CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                  "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
                  retried_count, elapsed_time);
              SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_REQUEST_TIMEOUT, "OktaConnectionFailed: timeout reached", SF_SQLSTATE_GENERAL_ERROR);
              goto cleanup;
          }
          //Need to increase retried_count on the authentication level
          retried_count++;
          CXX_LOG_TRACE("sf", "Connection", "Connect",
              "Retry on getting SAML response with one time token renewed for %d times "
              "with updated retryTimeout = %d",
              retried_count, retry_timeout - elapsed_time);
          isRetry = true;
          goto cleanup;
      }
      m_samlResponse = snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(resp, "data"));;
  cleanup:
      free_curl_desc(curl_desc);
      snowflake_cJSON_Delete(resp);
      if (isRetry) {
          RETRY_THROW(elapsed_time, retried_count);
      }
      m_connection->retry_timeout -= elapsed_time;
      m_connection->retry_count = retried_count;
      if (err->error_code != SF_STATUS_SUCCESS) {
          AUTH_THROW(err);
      }
  }

  void AuthenticatorOKTA::authenticate()
  {
      // 1. get authenticator info
      getIDPInfo();

      SF_ERROR_STRUCT* err = &m_connection->error;
      // 2. verify ssoUrl and tokenUrl contains same prefix
      if (!SFURL::urlHasSamePrefix(tokenURLStr, m_connection->authenticator))
      {
          CXX_LOG_ERROR("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
              "The specified authenticator is not supported, "
              "authenticator=%s, token url=%s, sso url=%s",
              m_connection->authenticator, tokenURLStr.c_str(), ssoURLStr.c_str());
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_BAD_REQUEST, "SFAuthenticatorVerificationFailed: the token URL does not have the same prefix with the authenticator", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW(err->msg);
      }


      // 3. get one time token from okta
      jsonObject_t dataMap;
      dataMap.clear();
      dataMap["username"] = picojson::value(m_connection->user);
      dataMap["password"] = picojson::value(m_connection->password);
      while (true)
      {
          getOneTimeToken(dataMap);
          oneTimeToken = dataMap["sessionToken"].get<std::string>();
          if (oneTimeToken.empty()) {
              oneTimeToken = dataMap["cookieToken"].get<std::string>();
          }
          // 4. get SAML response
          try {
              getSAMLResponse();
              break;
          }
          catch (Exception::RenewTimeoutException& e)
          {
              int64 elapsedSeconds = e.getElapsedSeconds();
              int64 retryTimeout = get_retry_timeout(m_connection);

              if (elapsedSeconds >= retryTimeout)
              {
                  CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponseUsingOkta",
                      "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
                      retryTimeout, elapsedSeconds);

                  SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_REQUEST_TIMEOUT, "OktaConnectionFailed: timeout reached", SF_SQLSTATE_GENERAL_ERROR);
                  AUTH_THROW(err);
              }

              int8 retriedCount = e.getRetriedCount();
              CXX_LOG_TRACE("sf", "Connection", "Connect",
                  "Retry on getting SAML response with one time token renewed for %d times "
                  "with updated retryTimeout = %d",
                  retriedCount, retryTimeout - elapsedSeconds);
              m_connection->retry_timeout -= elapsedSeconds;
              m_connection->retry_count = retriedCount;

          }
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
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_REQUEST_TIMEOUT, "SFSamlResponseVerificationFailed", SF_SQLSTATE_GENERAL_ERROR);
      }
  }

  void AuthenticatorOKTA::updateDataMap(jsonObject_t& dataMap)
  {
      dataMap.erase("LOGIN_NAME");
      dataMap.erase("PASSWORD");
      dataMap.erase("EXT_AUTHN_DUO_METHOD");
      dataMap["RAW_SAML_RESPONSE"] = picojson::value(m_samlResponse);
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

  SFURL AuthenticatorOKTA::getServerURLSync()
  {
      SFURL url = SFURL().scheme(m_connection->protocol)
          .host(m_connection->host)
          .port(m_connection->port);

      return url;
  }
} // namespace Client
} // namespace Snowflake
