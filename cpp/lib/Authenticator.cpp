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
#include "memory.h"
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
  throw ::AuthException(msg);  \
}

#define RETRY_THROW(elapsedSeconds, retriedCount)\
{                                                \
  throw ::RenewTimeoutException(elapsedSeconds, retriedCount, false);\
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

  SF_STATUS STDCALL auth_initialize(SF_CONNECT *conn)
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

  int64 auth_get_renew_timeout(SF_CONNECT *conn)
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

  SF_STATUS STDCALL auth_authenticate(SF_CONNECT *conn)
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

  void auth_update_json_body(SF_CONNECT *conn, cJSON* body)
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

  void auth_renew_json_body(SF_CONNECT *conn, cJSON* body)
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

  void STDCALL auth_terminate(SF_CONNECT *conn)
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

  void IDPAuthenticator::getIDPInfo()
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

      SFURL connectURL = getServerURLSync().path("/session/authenticator-request");
      jsonObject_t authnData, respData;
      authnData["data"] = value(dataMap);

      int64 maxRetryCount = get_login_retry_count(m_connection);
      int64 retryTimeout = get_login_timeout(m_connection);
      int64 renewTimeout = auth_get_renew_timeout(m_connection);
      int64 curlTimeout = m_connection->network_timeout;
      bool injectCURLTimeout = false;
      bool isNewRetry = true;
      int8* retriedCount = &m_connection->retry_count;

      curl_post_call(connectURL, authnData, respData, curlTimeout, retryTimeout, 0, maxRetryCount, injectCURLTimeout, renewTimeout, retriedCount, isNewRetry);
      jsonObject_t& data = respData["data"].get<jsonObject_t>();
      tokenURLStr = data["tokenUrl"].get<std::string>();
      ssoURLStr = data["ssoUrl"].get<std::string>();
  }

  SFURL IDPAuthenticator::getServerURLSync()
  {
      SFURL url = SFURL().scheme(m_connection->protocol)
          .host(m_connection->host)
          .port(m_connection->port);

      return url;
  }

  void IDPAuthenticator::curl_post_call(SFURL& url, const jsonObject_t& obj, jsonObject_t& resp, int64 curlTimeout,
      int64 retryTimeout, int8 flags, int8 maxRetryCount, bool injectCURLTimeout, int64 renewTimeout,
      int8 *retriedCount, bool isNewRetry)
  {
      std::string destination = url.toString();
      void* curl_desc;
      CURL* curl;
      curl_desc = get_curl_desc_from_pool(destination.c_str(), m_connection->proxy, m_connection->no_proxy);
      curl = get_curl_from_desc(curl_desc);
      SF_ERROR_STRUCT *err = &m_connection->error;

      int64 elapsedTime = 0;

      // add headers for account and authentication
      SF_HEADER* httpExtraHeaders = sf_header_create();
      std::string s_body = value(obj).serialize();
      cJSON* resp_data = NULL;

      httpExtraHeaders->use_application_json_accept_type = SF_BOOLEAN_TRUE;
      if (!create_header(m_connection, httpExtraHeaders, &m_connection->error)) {
          log_trace("sf", "IDPAuthenticator",
              "post_curl_call",
              "Failed to create the header for the request to get the token URL and the SSO URL");
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_GENERAL, "OktaConnectionFailed: failed to create the header", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW(err);
          goto cleanup;
      }

      if (!::curl_post_call(m_connection, curl, (char*) destination.c_str(), httpExtraHeaders, (char*)s_body.c_str(),
          &resp_data, err, renewTimeout, maxRetryCount, retryTimeout, &elapsedTime,
          retriedCount, NULL, SF_BOOLEAN_TRUE))
      {
          log_info("sf", "IDPAuthenticator", "post_curl_call",
              "Fail to get authenticator info, response body=%s\n",
              snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(resp_data, "data")));
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_GENERAL, "SFConnectionFailed", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW(err);
          goto cleanup;
      }

      if (elapsedTime >= retryTimeout)
      {
          CXX_LOG_WARN("sf", "IDPAuthenticator", "post_curl_call",
              "timeout reached: %d, elapsed time: %d",
              retryTimeout, elapsedTime);

          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_REQUEST_TIMEOUT, "OktaConnectionFailed: timeout reached", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW(err);
      }

      //deduct elasped time from the retry timeout
      m_connection->retry_timeout -= elapsedTime;
      cJSONtoPicoJson(resp_data, resp);
  cleanup:
      sf_header_destroy(httpExtraHeaders);
      snowflake_cJSON_Delete(resp_data);
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
    catch (AuthException& e) {
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

  void AuthenticatorJWT::updateDataMap(jsonObject_t& dataMap)
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
      SF_CONNECT *connection) : IDPAuthenticator(connection)
  {
      // nop
  }

  AuthenticatorOKTA::~AuthenticatorOKTA()
  {
      // nop
  }

  void AuthenticatorOKTA::curl_get_call(SFURL& url, jsonObject_t& resp, int64 curlTimeout,
      int64 retryTimeout, int8 flags, int8 maxRetryCount, bool injectCURLTimeout, int64 renewTimeout,
      int8* retriedCount, bool isNewRetry, bool parseJSON, std::string& rawData)
  {
      bool isRetry = false;

      std::string destination = url.toString();
      void* curl_desc;
      CURL* curl;
      curl_desc = get_curl_desc_from_pool(destination.c_str(), m_connection->proxy, m_connection->no_proxy);
      curl = get_curl_from_desc(curl_desc);

      SF_ERROR_STRUCT *err = &m_connection->error;
      int64 elapsedTime = 0;

      NON_JSON_RESP* raw_resp = new NON_JSON_RESP;
      raw_resp->write_callback = non_json_resp_write_callback;
      RAW_JSON_BUFFER buf = { NULL,0 };
      raw_resp->buffer = (void*)&buf;

      // add headers for account and authentication
      SF_HEADER* httpExtraHeaders = sf_header_create();

      httpExtraHeaders->use_application_json_accept_type = SF_BOOLEAN_TRUE;
      if (!create_header(m_connection, httpExtraHeaders, &m_connection->error)) {
          log_trace("sf", "AuthenticatorOKTA",
              "get_curl_call",
              "Failed to create the header for the request to get onetime token");
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_GENERAL, "OktaConnectionFailed: failed to create the header", SF_SQLSTATE_GENERAL_ERROR);
          goto cleanup;
      }

      if (!http_perform(curl, GET_REQUEST_TYPE, (char*)destination.c_str(), httpExtraHeaders, NULL, NULL, raw_resp,
              curlTimeout, SF_BOOLEAN_FALSE, err,
              m_connection->insecure_mode, m_connection->ocsp_fail_open,
              m_connection->retry_on_curle_couldnt_connect_count,
              renewTimeout, maxRetryCount, &elapsedTime, retriedCount, NULL, SF_BOOLEAN_FALSE,
              m_connection->proxy, m_connection->no_proxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE)) 
      {
          //Fail to get the saml response. Retry.
              isRetry = true;
              goto cleanup;
      }

      if (elapsedTime >= retryTimeout)
      {
          CXX_LOG_WARN("sf", "AuthenticatorOKTA", "get_curl_call",
              "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
              retryTimeout, elapsedTime);

          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_REQUEST_TIMEOUT, "OktaConnectionFailed: timeout reached", SF_SQLSTATE_GENERAL_ERROR);
          goto cleanup;
      }

      rawData = buf.buffer;
  cleanup:
      sf_header_destroy(httpExtraHeaders);
      free_curl_desc(curl_desc);
      SF_FREE(raw_resp);
      if (isRetry) {
          RETRY_THROW(elapsedTime, *retriedCount);
      }
      m_connection->retry_timeout -= elapsedTime;
      m_connection->retry_count = *retriedCount;
      if (err->error_code != SF_STATUS_SUCCESS) {
          AUTH_THROW(err);
      }
  }

  void AuthenticatorOKTA::getOneTimeToken()
  {
      // When renewTimeout < 0, throws Exception
      // for each retry attempt so we can renew the one time token
      int64 retry_timeout = get_login_timeout(m_connection);
      int8 retry_max_count = get_login_retry_count(m_connection);
      int8* retried_count = &m_connection->retry_count;
      int64 renewTimeout = auth_get_renew_timeout(m_connection);
      int64 curlTimeout = m_connection->network_timeout;

      SFURL tokenURL = SFURL::parse(tokenURLStr);

      jsonObject_t dataMap,respData;
      dataMap["username"] = picojson::value(m_connection->user);
      dataMap["password"] = picojson::value(m_connection->password);

      try {
          curl_post_call(tokenURL, dataMap, respData, curlTimeout, retry_timeout, 8, retry_max_count, false, renewTimeout, retried_count, false);
      }
      catch (...)
      {
          CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getOneTimeToken",
              "Fail to get one time token response, response body=%s",
              picojson::value(respData).serialize().c_str());
          SET_SNOWFLAKE_ERROR(&m_connection->error, SF_STATUS_ERROR_BAD_REQUEST, "OktaConnectionFailed", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW("Failed to get the one time token from Okta authentication.")
      }

      oneTimeToken = respData["sessionToken"].get<std::string>();
      if (oneTimeToken.empty()) {
          oneTimeToken = respData["cookieToken"].get<std::string>();
      }
  }

  void AuthenticatorOKTA::getSAMLResponse()
  {
      bool isRetry = false;
      int64 retry_timeout = get_login_timeout(m_connection);
      int64 retry_max_count = get_login_retry_count(m_connection);
      int64 elapsedTime = 0;
      int8* retried_count = &m_connection->retry_count;
      int64 renewTimeout = auth_get_renew_timeout(m_connection);
      int64 curlTimeout = m_connection->network_timeout;

      jsonObject_t resp;
      SFURL sso_url = SFURL::parse(ssoURLStr);
      sso_url.addQueryParam("onetimetoken", oneTimeToken);
      curl_get_call(sso_url, resp, curlTimeout, retry_timeout, 8, retry_max_count, false, renewTimeout, retried_count, false, false, m_samlResponse);
  }

  void AuthenticatorOKTA::authenticate()
  {
      // 1. get authenticator info
      getIDPInfo();

      SF_ERROR_STRUCT *err = &m_connection->error;
      // 2. verify ssoUrl and tokenUrl contains same prefix
      if (!SFURL::urlHasSamePrefix(tokenURLStr, m_connection->authenticator))
      {
          CXX_LOG_ERROR("sf", "AuthenticatorOKTA", "authenticate",
              "The specified authenticator is not supported, "
              "authenticator=%s, token url=%s, sso url=%s",
              m_connection->authenticator, tokenURLStr.c_str(), ssoURLStr.c_str());
          SET_SNOWFLAKE_ERROR(err, SF_STATUS_ERROR_BAD_REQUEST, "SFAuthenticatorVerificationFailed: the token URL does not have the same prefix with the authenticator", SF_SQLSTATE_GENERAL_ERROR);
          AUTH_THROW(err);
      }

      // 3. get one time token from okta
      while (true)
      {
          getOneTimeToken();
          // 4. get SAML response
          try {
              getSAMLResponse();
              break;
          }
          catch (RenewTimeoutException& e)
          {
              int64 elapsedSeconds = e.getElapsedSeconds();
              int64 retryTimeout = get_retry_timeout(m_connection);

              if (elapsedSeconds >= retryTimeout)
              {
                  CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getSamlResponse",
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
          CXX_LOG_ERROR("sf", "AuthenticatorOKTA", "authenticate",
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
} // namespace Client
} // namespace Snowflake
