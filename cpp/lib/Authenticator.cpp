/*
 * Copyright (c) 2025 Snowflake Computing, Inc. All rights reserved.
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

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <connection.h>
#include "curl_desc_pool.h"
#include "cJSON.h"
#include "memory.h"

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

    if (AUTH_OAUTH == getAuthenticatorType(conn->authenticator)) 
    {
        cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");
        if (!data)
        {
            data = snowflake_cJSON_CreateObject();
            snowflake_cJSON_AddItemToObject(body, "data", data);
        }

        snowflake_cJSON_DeleteItemFromObject(data, "AUTHENTICATOR");
        snowflake_cJSON_AddStringToObject(data, "AUTHENTICATOR", SF_AUTHENTICATOR_OAUTH);
        snowflake_cJSON_DeleteItemFromObject(data, "TOKEN");
        snowflake_cJSON_AddStringToObject(data, "TOKEN", conn->oauth_token);
    }

    if (!conn || !conn->auth_object)
    {
      return;
    }

    try
    {
      jsonObject_t picoBody;
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

    try
    {
      delete static_cast<Snowflake::Client::IAuth::IAuthenticator*>(conn->auth_object);
      conn->auth_object = nullptr;
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

  void AuthenticatorJWT::updateDataMap(jsonObject_t& dataMap)
  {
    dataMap.erase("AUTHENTICATOR");
    dataMap.erase("TOKEN");
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
      m_idp = new CIDPAuthenticator(connection);

      m_password = m_connection->password;
      m_disableSamlUrlCheck = m_connection->disable_saml_url_check;
      m_appID = m_connection->application_name;
      m_appVersion = m_connection->application_version;
  }

  AuthenticatorOKTA::~AuthenticatorOKTA()
  {
      delete m_idp;
  }

  void AuthenticatorOKTA::authenticate()
  {
      IAuthenticatorOKTA::authenticate();
      if ((m_connection->error).error_code == SF_STATUS_SUCCESS && (isError() || m_idp->isError()))
      {
          const char* err = isError() ? getErrorMessage() : m_idp->getErrorMessage();
          SET_SNOWFLAKE_ERROR(&m_connection->error, SF_STATUS_ERROR_GENERAL, err, SF_SQLSTATE_GENERAL_ERROR);
      }
  }

  void AuthenticatorOKTA::updateDataMap(jsonObject_t& dataMap)
  {
      dataMap.erase("LOGIN_NAME");
      dataMap.erase("PASSWORD");
      dataMap.erase("EXT_AUTHN_DUO_METHOD");
      dataMap.erase("AUTHENTICATOR");
      dataMap.erase("TOKEN");

      IAuthenticatorOKTA::updateDataMap(dataMap);
  }

  CIDPAuthenticator::CIDPAuthenticator(SF_CONNECT* conn) : m_connection(conn)
  {
      m_account = m_connection->account;
      m_authenticator = m_connection->authenticator;
      m_user = m_connection->user;
      m_port = m_connection->port;
      m_host = m_connection->host;
      m_protocol = m_connection->protocol;
      m_retriedCount = 0;
      m_retryTimeout = get_retry_timeout(m_connection);
  }

  CIDPAuthenticator::~CIDPAuthenticator()
  {
      // nop
  }

  bool CIDPAuthenticator::curlPostCall(SFURL& url, const jsonObject_t& obj, jsonObject_t& resp)
  {
      bool ret = true;
      std::string destination = url.toString();
      void* curl_desc;
      CURL* curl;
      curl_desc = get_curl_desc_from_pool(destination.c_str(), m_connection->proxy, m_connection->no_proxy);
      curl = get_curl_from_desc(curl_desc);
      SF_ERROR_STRUCT* err = &m_connection->error;

      int64 elapsedTime = 0;
      int8 maxRetryCount = get_login_retry_count(m_connection);
      int64 renewTimeout = auth_get_renew_timeout(m_connection);

      // add headers for account and authentication
      SF_HEADER* httpExtraHeaders = sf_header_create();
      std::string s_body = value(obj).serialize();
      cJSON* resp_data = NULL;
      httpExtraHeaders->use_application_json_accept_type = SF_BOOLEAN_TRUE;
      if (!create_header(m_connection, httpExtraHeaders, &m_connection->error)) {
          CXX_LOG_TRACE("sf", "Authenticator",
              "post_curl_call",
              "Failed to create the header for the request to get the token URL and the SSO URL");
          m_errMsg = "OktaConnectionFailed: failed to create the header";
          ret = false;
      }

      if (ret)
      {
          if (!curl_post_call(m_connection, curl, (char*)destination.c_str(), httpExtraHeaders, (char*)s_body.c_str(),
              &resp_data, err, renewTimeout, maxRetryCount, m_retryTimeout, &elapsedTime,
              &m_retriedCount, NULL, SF_BOOLEAN_TRUE))
          {
              CXX_LOG_INFO("sf", "Authenticator", "post_curl_call",
                  "post call failed, response body=%s\n",
                  snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(resp_data, "data")));
              m_errMsg = "SFConnectionFailed: Fail to get one time token";
              ret = false;
          }
          else
          {
              cJSONtoPicoJson(resp_data, resp);
          }
      }

      if (ret && elapsedTime >= m_retryTimeout)
      {
          CXX_LOG_WARN("sf", "Authenticator", "get_curl_call",
              "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
              m_retryTimeout, elapsedTime);

          m_errMsg = "OktaConnectionFailed: timeout reached";
          ret = false;
      }

      sf_header_destroy(httpExtraHeaders);
      free_curl_desc(curl_desc);
      snowflake_cJSON_Delete(resp_data);
      return ret;
  }

  bool CIDPAuthenticator::curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& rawData, bool& isRetry)
  {
      isRetry = false;
      bool ret = true;
      sf_bool isHttpSuccess = SF_BOOLEAN_TRUE;
      int64 maxRetryCount = get_login_retry_count(m_connection);
      int64 elapsedTime = 0;
      int64 renewTimeout = auth_get_renew_timeout(m_connection);
      int64 curlTimeout = m_connection->network_timeout;

      std::string destination = url.toString();
      void* curl_desc;
      CURL* curl;
      curl_desc = get_curl_desc_from_pool(destination.c_str(), m_connection->proxy, m_connection->no_proxy);
      curl = get_curl_from_desc(curl_desc);

      SF_ERROR_STRUCT* err = &m_connection->error;

      NON_JSON_RESP* raw_resp = (NON_JSON_RESP*)SF_MALLOC(sizeof(NON_JSON_RESP));
      raw_resp->write_callback = non_json_resp_write_callback;
      RAW_CHAR_BUFFER buf = { NULL,0 };
      raw_resp->buffer = (void*)&buf;
      cJSON* resp_data = NULL;

      // add headers for account and authentication
      SF_HEADER* httpExtraHeaders = sf_header_create();
      httpExtraHeaders->use_application_json_accept_type = SF_BOOLEAN_TRUE;
      if (!create_header(m_connection, httpExtraHeaders, &m_connection->error))
      {
          CXX_LOG_TRACE("sf", "Authenticator",
              "get_curl_call",
              "Failed to create the header for the request to get onetime token");
          m_errMsg = "OktaConnectionFailed: failed to create the header";
          ret = false;
      }

      if (ret)
      {
          if (parseJSON) 
          {
              isHttpSuccess = http_perform(curl, GET_REQUEST_TYPE, (char*)destination.c_str(), httpExtraHeaders, NULL, NULL, &resp_data,
                  raw_resp, NULL, curlTimeout, SF_BOOLEAN_FALSE, err,
                  m_connection->insecure_mode, m_connection->ocsp_fail_open,
                  m_connection->retry_on_curle_couldnt_connect_count,
                  renewTimeout, maxRetryCount, &elapsedTime, &m_retriedCount, NULL, SF_BOOLEAN_FALSE,
                  m_connection->proxy, m_connection->no_proxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
          }
          else
          {
              isHttpSuccess = http_perform(curl, GET_REQUEST_TYPE, (char*)destination.c_str(), httpExtraHeaders, NULL, NULL, NULL,
                  raw_resp, NULL, curlTimeout, SF_BOOLEAN_FALSE, err,
                  m_connection->insecure_mode, m_connection->ocsp_fail_open,
                  m_connection->retry_on_curle_couldnt_connect_count,
                  renewTimeout, maxRetryCount, &elapsedTime, &m_retriedCount, NULL, SF_BOOLEAN_FALSE,
                  m_connection->proxy, m_connection->no_proxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
          }

          if (!isHttpSuccess)
          {
              //Fail to get the saml response. Retry.
              isRetry = true;
              ret = false;
          }
          else
          {
              if (parseJSON)
              {
                  cJSONtoPicoJson(resp_data, resp);
              }
              else 
              {
                  rawData = buf.buffer;
              }
          }
      }

      if (ret && elapsedTime >= m_retryTimeout)
      {
          CXX_LOG_WARN("sf", "AuthenticatorOKTA", "get_curl_call",
              "Fail to get SAML response, timeout reached: %d, elapsed time: %d",
              m_retryTimeout, elapsedTime);

          m_errMsg = "OktaConnectionFailed: timeout reached";
          ret = false;
      }

      sf_header_destroy(httpExtraHeaders);
      free_curl_desc(curl_desc);
      SF_FREE(raw_resp);
      return ret;
  }
} // namespace Client
} // namespace Snowflake
