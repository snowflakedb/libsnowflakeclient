#include <string>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <regex>
#include <functional>
#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "Authenticator.hpp"
#include "AuthenticatorOAuth.hpp"
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
#include <CoreFoundation/CoreFoundation.h>
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

#define SOCKET_BUFFER_SIZE 20000

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
    if (strcasecmp(authenticator, SF_AUTHENTICATOR_EXTERNAL_BROWSER) == 0)
    {
      return AUTH_EXTERNALBROWSER;
    }
    if (strcasecmp(authenticator, SF_AUTHENTICATOR_PAT) == 0)
    {
        return AUTH_PAT;
    }
    if (strcasecmp(authenticator, SF_AUTHENTICATOR_OAUTH_AUTHORIZATION_CODE) == 0)
    {
        return AUTH_OAUTH_AUTHORIZATION_CODE;
    }
    if (strcasecmp(authenticator, SF_AUTHENTICATOR_OAUTH_CLIENT_CREDENTIALS) == 0)
    {
        return AUTH_OAUTH_CLIENT_CREDENTIALS;
    }
    if (strcasecmp(authenticator, "test") == 0)
    {
        return AUTH_TEST;
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
      if (AUTH_EXTERNALBROWSER == auth_type)
      {
          conn->auth_object = static_cast<Snowflake::Client::IAuthenticator*>(
              new Snowflake::Client::AuthenticatorExternalBrowser(conn));
      }
      if (AUTH_OKTA == auth_type)
      {
        conn->auth_object = static_cast<Snowflake::Client::IAuthenticator*>(
                              new Snowflake::Client::AuthenticatorOKTA(conn));
      }
      if (AUTH_OAUTH_AUTHORIZATION_CODE == auth_type || AUTH_OAUTH_CLIENT_CREDENTIALS == auth_type)
      {
          conn->auth_object = static_cast<Snowflake::Client::IAuthenticator*>(
              new Snowflake::Client::AuthenticatorOAuth(conn,
                  nullptr, nullptr));
      }
      if (AUTH_TEST == auth_type)
      {
          conn->auth_object = static_cast<Snowflake::Client::IAuthenticator*>(
              new Snowflake::Client::AuthenticatorTest(conn));
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
    if (!conn || !conn->auth_object || conn->sso_token != NULL)
    {
      return SF_STATUS_SUCCESS;
    }

    try
    {
      static_cast<Snowflake::Client::IAuthenticator*>(conn->auth_object)->authenticate();

      if (conn->error.error_code != SF_STATUS_SUCCESS)
      {
        return SF_STATUS_ERROR_GENERAL;
      }
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

  void auth_update_json_body(SF_CONNECT* conn, cJSON* body)
  {
      cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");
      if (!data)
      {
          data = snowflake_cJSON_CreateObject();
          snowflake_cJSON_AddItemToObject(body, "data", data);
      }
      auto authenticator = getAuthenticatorType(conn->authenticator);
      if (AUTH_OAUTH == authenticator || AUTH_PAT == authenticator)

      {
          snowflake_cJSON_DeleteItemFromObject(data, "AUTHENTICATOR");
          snowflake_cJSON_DeleteItemFromObject(data, "TOKEN");

          if (AUTH_PAT == authenticator) {
              snowflake_cJSON_AddStringToObject(data, "AUTHENTICATOR", SF_AUTHENTICATOR_PAT);
              snowflake_cJSON_AddStringToObject(data, "TOKEN", conn->programmatic_access_token);
          }
          else {
              snowflake_cJSON_AddStringToObject(data, "AUTHENTICATOR", SF_AUTHENTICATOR_OAUTH);
              snowflake_cJSON_AddStringToObject(data, "TOKEN", conn->oauth_token);
          }
      }

      if (conn->sso_token)
      {
          snowflake_cJSON_DeleteItemFromObject(data, "AUTHENTICATOR");
          snowflake_cJSON_AddStringToObject(data, "TOKEN", conn->sso_token);
          snowflake_cJSON_AddStringToObject(data, "AUTHENTICATOR", SF_AUTHENTICATOR_ID_TOKEN);
          secure_storage_free_credential(conn->sso_token);
      }

      if (conn->mfa_token)
      {
          snowflake_cJSON_AddStringToObject(data, "TOKEN", conn->mfa_token);
          secure_storage_free_credential(conn->mfa_token);
      }

      if (!conn || !conn->auth_object || conn->sso_token != NULL)
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
      m_user = m_connection->user;
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
          CXX_LOG_TRACE("sf::CIDPAuthenticator::post_curl_call::Failed to create the header for the request to get the token URL and the SSO URL");
          m_errMsg = "OktaConnectionFailed: failed to create the header.";
          ret = false;
      }

      if (ret)
      {
          if (!curl_post_call(m_connection, curl, (char*)destination.c_str(), httpExtraHeaders, (char*)s_body.c_str(),
              &resp_data, err, renewTimeout, maxRetryCount, m_retryTimeout, &elapsedTime,
              &m_retriedCount, NULL, SF_BOOLEAN_TRUE))
          {
              CXX_LOG_INFO("sf::CIDPAuthenticator::post_curl_call::post call failed, response body=%s\n",
                  snowflake_cJSON_Print(snowflake_cJSON_GetObjectItem(resp_data, "data")));
              m_errMsg = "SFConnectionFailed: Fail to get one time token.";
              ret = false;
          }
          else
          {
              cJSONtoPicoJson(resp_data, resp);
          }
      }

      if (ret && elapsedTime >= m_retryTimeout)
      {
          CXX_LOG_WARN("sf::CIDPAuthenticator::get_curl_call::Fail to get SAML response, timeout reached: %d, elapsed time: %d",
              m_retryTimeout, elapsedTime);

          m_errMsg = "OktaConnectionFailed: timeout reached.";
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
          CXX_LOG_TRACE("sf::CIDPAuthenticator::curlGetCall::Failed to create the header for the request to get onetime token");
          m_errMsg = "OktaConnectionFailed: failed to create the header.";
          ret = false;
      }

      if (ret)
      {
          if (parseJSON) 
          {
              isHttpSuccess = http_perform(curl, GET_REQUEST_TYPE, (char*)destination.c_str(), httpExtraHeaders, NULL, NULL, &resp_data,
                                           raw_resp, NULL, curlTimeout, SF_BOOLEAN_FALSE, err,
                                           m_connection->insecure_mode, m_connection->ocsp_fail_open,
                                           m_connection->crl_check, m_connection->crl_advisory, m_connection->crl_allow_no_crl,
                                           m_connection->crl_disk_caching, m_connection->crl_memory_caching,
                                           m_connection->crl_download_timeout,
                                           m_connection->retry_on_curle_couldnt_connect_count, renewTimeout, maxRetryCount, &elapsedTime, &m_retriedCount, NULL,
                                           SF_BOOLEAN_FALSE, m_connection->proxy, m_connection->no_proxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
          }
          else
          {
              isHttpSuccess = http_perform(curl, GET_REQUEST_TYPE, (char*)destination.c_str(), httpExtraHeaders, NULL, NULL, NULL,
                                           raw_resp, NULL, curlTimeout, SF_BOOLEAN_FALSE, err,
                                           m_connection->insecure_mode, m_connection->ocsp_fail_open,
                                           m_connection->crl_check, m_connection->crl_advisory, m_connection->crl_allow_no_crl,
                                           m_connection->crl_disk_caching, m_connection->crl_memory_caching,
                                           m_connection->crl_download_timeout,
                                           m_connection->retry_on_curle_couldnt_connect_count, renewTimeout, maxRetryCount, &elapsedTime, &m_retriedCount, NULL,
                                           SF_BOOLEAN_FALSE, m_connection->proxy, m_connection->no_proxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
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
          CXX_LOG_WARN("sf::CIDPAuthenticator::get_curl_call::Fail to get SAML response, timeout reached: %d, elapsed time: %d",
              m_retryTimeout, elapsedTime);

          m_errMsg = "OktaConnectionFailed: timeout reached.";
          ret = false;
      }

      sf_header_destroy(httpExtraHeaders);
      free_curl_desc(curl_desc);
      SF_FREE(raw_resp);
      return ret;
  }

  AuthenticatorExternalBrowser::AuthenticatorExternalBrowser(
      SF_CONNECT* connection, IAuthWebServer* authWebServer) : m_connection(connection)
  {
      m_proofKey = "";
      m_token = "";
      m_consentCacheIdToken = true;
      m_origin = "";
      m_authWebServer = authWebServer;

      if (m_authWebServer == NULL)
      {
          m_authWebServer = new AuthWebServer();
      }
      m_idp = new CIDPAuthenticator(m_connection);

      m_connection->disable_console_login ?  m_user = "" : m_user = m_connection->user;
      m_browser_response_timeout = connection->browser_response_timeout;
      m_disable_console_login = connection->disable_console_login;
  };

  AuthenticatorExternalBrowser::~AuthenticatorExternalBrowser()
  {
      if (m_authWebServer != NULL)
      {
          delete m_authWebServer;
      }
      if (m_idp != NULL)
      {
          delete m_idp;
      }
  }

  void AuthenticatorExternalBrowser::authenticate()
  {
      IAuthenticatorExternalBrowser::authenticate();
      if (m_authWebServer->isError())
      {
          SET_SNOWFLAKE_ERROR(&m_connection->error, SF_STATUS_ERROR_GENERAL, m_authWebServer->getErrorMessage(), SF_SQLSTATE_GENERAL_ERROR);
      }
      else if (isError())
      {
          SET_SNOWFLAKE_ERROR(&m_connection->error, SF_STATUS_ERROR_GENERAL, getErrorMessage(), SF_SQLSTATE_GENERAL_ERROR);
      }
#ifdef _WIN32
      else if (m_authWinSock.isError())
      {
          SET_SNOWFLAKE_ERROR(&m_connection->error, SF_STATUS_ERROR_GENERAL, m_authWinSock.getErrorMessage(), SF_SQLSTATE_GENERAL_ERROR);
      }
#endif
  }

  /**
* Constructor for AuthWebServer
*/
  AuthWebServer::AuthWebServer() : m_socket_descriptor(0),
      m_port(0),
      m_saml_token(""),
      m_consent_cache_id_token(true),
      m_origin(""),
      m_timeout(SF_BROWSER_RESPONSE_TIMEOUT)
  {
      // nop
  }

  /**
   * Destructor for AuthWebServer
   */
  AuthWebServer::~AuthWebServer()
  {
      // nop
  }


  /**
   * Start web server that accepts SAML token from Snowflake
   */
  void AuthWebServer::start()
  {
      m_socket_desc_web_client = 0;
      m_socket_descriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if ((int)m_socket_descriptor < 0)
      {
          CXX_LOG_ERROR("sf::AuthWebServer::start::Failed to start web server. Could not create a socket.");
          m_errMsg = "SFAuthWebBrowserFailed: Failed to start web server. Could not create a socket.";
          return;
      }

      struct sockaddr_in recv_server;
      memset((char*)&recv_server, 0, sizeof(struct sockaddr_in));
      recv_server.sin_family = AF_INET;
      inet_pton(AF_INET, "127.0.0.1", &recv_server.sin_addr.s_addr);
      recv_server.sin_port = 0; // ephemeral port
      if (bind(m_socket_descriptor, (struct sockaddr*)&recv_server,
          sizeof(struct sockaddr_in)) < 0)
      {
          CXX_LOG_ERROR("sf::AuthWebServer::start::Failed to start web server. Failed to start web server. Could not bind a port.");
          m_errMsg = "SFAuthWebBrowserFailed: Failed to start web server. Could not bind a port.";
          return;
      }
      if (listen(m_socket_descriptor, 0) < 0)
      {
          CXX_LOG_ERROR("sf::AuthWebServer::start::Failed to start web server. Failed to start web server. Could not listen a port.");
          m_errMsg = "SFAuthWebBrowserFailed: Failed to start web server. Could not listen a port.";
          return;
      }
      socklen_t len = sizeof(recv_server);
      if (getsockname(m_socket_descriptor,
          (struct sockaddr*)&recv_server, &len) < 0)
      {
          CXX_LOG_ERROR("sf::AuthWebServer::start::Failed to start web server. Failed to start web server. Could not get the port.");
          m_errMsg = "SFAuthWebBrowserFailed: Failed to start web server. Could not get the port.";
          return;
      }
      m_port = ntohs(recv_server.sin_port);
      CXX_LOG_INFO("sf::AuthWebServer::start::Web Server successfully started with port %d.", m_port);
  }

  int AuthWebServer::start(std::string host, int port, std::string path) {
      SF_UNUSED(host);
      SF_UNUSED(port);
      SF_UNUSED(path);
      return 0;
  };

  /**
   * Stop web server
   */
  void AuthWebServer::stop()
  {
      if ((int)m_socket_desc_web_client > 0)
      {
#ifndef _WIN32
          shutdown(m_socket_desc_web_client, SHUT_RDWR);
          int ret = close(m_socket_desc_web_client);
#else
          shutdown(m_socket_desc_web_client, SD_BOTH);
          int ret = closesocket(m_socket_desc_web_client);
#endif
          if (ret < 0)
          {
              CXX_LOG_ERROR("sf::AuthWebServer::stop::Failed to accept the SAML token.");
              m_errMsg = "SFAuthWebBrowserFailed:Failed to accept the SAML token.";
              return;
          }
      }
      m_socket_desc_web_client = 0;

      if ((int)m_socket_descriptor > 0)
      {
#ifndef _WIN32
          int ret = close(m_socket_descriptor);
#else
          int ret = closesocket(m_socket_descriptor);
#endif
          if (ret < 0)
          {
             CXX_LOG_ERROR("sf::AuthWebServer::stop:Failed to stop web server.");
             m_socket_descriptor = 0;
             m_errMsg = "SFAuthWebBrowserFailed.";
             return;
          }
      }
      m_socket_descriptor = 0;
  }

  /**
   * Get port number listening
   * @return port number
   */
  int AuthWebServer::getPort()
  {
      return m_port;
  }

  /**
   * Set the timeout for the web server.
   */
  void AuthWebServer::setTimeout(int timeout)
  {
      m_timeout = timeout;
  }

  void AuthWebServer::startAccept()
  {
      struct sockaddr_in client = { 0, 0, 0, 0 };
      memset((char*)&client, 0, sizeof(struct sockaddr_in));
      socklen_t len = sizeof(client);

      fd_set fd;
      timeval timeout;
      FD_ZERO(&fd);
      FD_SET(m_socket_descriptor, &fd);
      timeout.tv_sec = m_timeout;
      timeout.tv_usec = 0;
      int retVal = select(m_socket_descriptor + 1, &fd, NULL, NULL, &timeout);
      if (retVal > 0)
      {
          m_socket_desc_web_client = accept(
              m_socket_descriptor, (struct sockaddr*)&client, &len);
          if ((int)m_socket_desc_web_client < 0)
          {
              CXX_LOG_ERROR("sf::AuthWebServer::startAccept::Failed to receive SAML token. Could not accept a request.");
              m_errMsg = "Failed to receive SAML token. Could not accept a request.";
          }
      }
      else if (retVal == 0)
      {
          CXX_LOG_ERROR("sf::AuthWebServer::startAccept::Auth browser timed out.");
          m_errMsg = "SFAuthWebBrowserFailed. Auth browser timed out.";

      }
      else
      {
          CXX_LOG_ERROR("sf::AuthWebServer::startAccept::Failed to determine status of auth web server.");
          m_errMsg = "SFAuthWebBrowserFailed. Failed to determine status of auth web server.";
      }
  }

  bool AuthWebServer::receive()
  {
      bool is_options = false;
      char* mesg = new char[SOCKET_BUFFER_SIZE]();
      char* reqline;
      char* rest_mesg;
      int recvlen;

      if ((recvlen = (int)recv(m_socket_desc_web_client, mesg, SOCKET_BUFFER_SIZE, 0)) < 0)
      {
          CXX_LOG_ERROR("sf::AuthWebServer::receive::Failed to receive SAML token. Could not receive a request.");
          m_errMsg = "SFAuthWebBrowserFailed: Failed to receive SAML token. Could not receive a request.";
          return false;
      }
      reqline = sf_strtok(mesg, " \t\n", &rest_mesg);
      if (strncmp(reqline, "GET\0", 4) == 0)
      {
          parseAndRespondGetRequest(&rest_mesg);
      }
      else if (strncmp(reqline, "POST\0", 5) == 0)
      {
          parseAndRespondPostRequest(std::string(rest_mesg, (unsigned long)recvlen));
      }
      else if (strncmp(reqline, "OPTIONS\0", 8) == 0)
      {
          is_options = parseAndRespondOptionsRequest(std::string(rest_mesg, (unsigned long)recvlen));
      }
      else
      {
          CXX_LOG_ERROR("sf::AuthWebServer::receive::Failed to receive SAML token. Could not get HTTP request. err: %s.", reqline);
          m_errMsg = "SFAuthWebBrowserFailed: Not HTTP request.";
      }
      delete[] mesg;
      return is_options;
  }

  void AuthWebServer::parseAndRespondPostRequest(std::string response)
  {
      auto ret = splitString(response, '\n');
      if (ret.empty()) 
      {
          CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondPostRequest:No token parameter is found %s.",response.c_str());
          send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
          m_errMsg = "AuthWebServer:parseAndRespondPostRequest:No token parameter is found.";
          return;
      }
      if (m_origin.empty())
      {
          respond(ret[ret.size() - 1]);
      }
      else
      {
          jsonValue_t json;
          std::string& payload = ret[ret.size() - 1];
          std::string err;
          picojson::parse(json, payload.begin(), payload.end(), &err);
          if (!err.empty())
          {
              CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondPostRequest:Error in parsing JSON : % s, err : % s.", payload.c_str(), err.c_str());
              m_errMsg = "AuthWebServer:parseAndRespondPostRequest:Error in parsing JSON.";
              return;
          }
          respondJson(json);
      }
  }

  bool AuthWebServer::parseAndRespondOptionsRequest(std::string response)
  {
      std::string requested_header;
      auto ret = splitString(response, '\n');
      if (ret.empty())
      {
          CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondOptionsRequest:No token parameter is found. %s.",response.c_str());
          send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
          m_errMsg = "AuthWebServer:parseAndRespondOptionsRequest:No token parameter is found.";

          return false;
      }

      for (auto const& value : ret)
      {
          if (value.find("Access-Control-Request-Method") != std::string::npos)
          {
              auto v = value.substr(value.find(':') + 1);
              trim(v, ' ');
              if (v != "POST")
              {
                  CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondOptionsRequest:POST method is not requested. %s.",value.c_str());
                  send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
                  m_errMsg = "AuthWebServer:parseAndRespondOptionsRequest:POST method is not requested.";

                  return false;
              }
          }
          else if (value.find("Access-Control-Request-Headers") != std::string::npos)
          {
              requested_header = value.substr(value.find(':') + 1);
              trim(requested_header, ' ');
          }
          else if (value.find("Origin") != std::string::npos)
          {
              m_origin = value.substr(value.find(':') + 1);
              trim(m_origin, ' ');
          }
      }
      if (requested_header.empty() || m_origin.empty())
      {
          CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondOptionsRequest:no Access-Control-Request-Headers or Origin header. %s.",response.c_str());
          send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
          m_errMsg = "AuthWebServer:parseAndRespondOptionsRequest:no Access-Control-Request-Headers or Origin header.";

          return false;
      }
      std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()
      );
      char current_timestamp[50];
      std::time_t t = (time_t)ms.count() / 1000;
      std::tm tms;
      strftime(current_timestamp, sizeof(current_timestamp), "%a, %d %b %Y %H:%M:%S GMT", sf_gmtime(&t, &tms));

      std::stringstream buf;
      buf << "HTTP/1.0 200 OK" << "\r\n"
          << "Date: " << current_timestamp << "\r\n"
          << "Access-Control-Allow-Methods: POST, GET" << "\r\n"
          << "Access-Control-Allow-Headers: " << requested_header << "\r\n"
          << "Access-Control-Max-Age: 86400" << "\r\n"
          << "Access-Control-Allow-Origin: " << m_origin << "\r\n"
          << "\r\n\r\n";
      send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
      return true;
  }

  std::vector<std::string> AuthWebServer::splitString(const std::string& s, char delimiter)
  {
      std::vector<std::string> tokens;
      std::string token;
      std::istringstream tokenStream(s);
      while (std::getline(tokenStream, token, delimiter))
      {
          tokens.push_back(token);
      }
      return tokens;
  }

  void AuthWebServer::parseAndRespondGetRequest(char** rest_mesg)
  {
      char* path = sf_strtok(NULL, " \t", rest_mesg);
      char* protocol = sf_strtok(NULL, " \t\n", rest_mesg);
      if (strncmp(protocol, "HTTP/1.0", 8) != 0 &&
          strncmp(protocol, "HTTP/1.1", 8) != 0)
      {
          CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondGetRequest:Not HTTP request.");

          send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
          m_errMsg = "AuthWebServer:parseAndRespondGetRequest:Not HTTP request.";
          return;
      }

      if (strncmp(path, "/?", 2) != 0)
      {
          CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondGetRequest:No token parameter is found.");
          send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
          m_errMsg = "AuthWebServer:parseAndRespondGetRequest:No token parameter is found.";
          return;
      }
      respond(std::string(&path[2]));
  }

  void AuthWebServer::respondJson(picojson::value& json)
  {
      jsonObject_t& obj = json.get<picojson::object>();
      m_saml_token = obj["token"].get<std::string>();
      m_consent_cache_id_token = obj["consent"].get<bool>();

      jsonObject_t payloadBody;
      payloadBody["consent"] = picojson::value(m_consent_cache_id_token);
      auto payloadBodyString = picojson::value(payloadBody).serialize();

      std::stringstream buf;
      buf << "HTTP/1.0 200 OK" << "\r\n"
          << "Content-Type: text/html" << "\r\n"
          << "Content-Length: " << payloadBodyString.size() << "\r\n"
          << "Access-Control-Allow-Origin: " << m_origin << "\r\n"
          << "Vary: Accept-Encoding, Origin" << "\r\n"
          << "\r\n"
          << payloadBodyString;
      send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
  }

  void AuthWebServer::respond(std::string queryParameters)
  {
      auto params = splitQuery(queryParameters);
      for (auto& p : params)
      {
          if (p.first == "token")
          {
              m_saml_token = p.second;
              break;
          }
      }
      const char* message =
          "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"/>\n"
          "<title>SAML Response for Snowflake</title></head>\n"
          "<body>\n"
          "Your identity was confirmed and propagated to Snowflake "
          "ODBC driver. You can close this window now and go back where you "
          "started from.\n"
          "</body></html>";
      std::stringstream buf;
      buf << "HTTP/1.0 200 OK" << "\r\n"
          << "Content-Type: text/html" << "\r\n"
          << "Content-Length: " << strlen(message) << "\r\n\r\n"
          << message;
      send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
  }

  std::string AuthWebServer::unquote(std::string src)
  {
      std::string ret;
      char ch;
      int i, ii;
      for (i = 0; i < (int)src.length(); i++)
      {
          if (src[i] == '%')
          {
              sf_sscanf(src.substr((unsigned long)(i + 1), 2).c_str(), "%x", &ii);
              ch = static_cast<char>(ii);
              ret += ch;
              i = i + 2;
          }
          else
          {
              ret += src[i];
          }
      }
      return ret;
  }

  std::vector<std::pair<std::string, std::string>> AuthWebServer::splitQuery(std::string query)
  {
      std::vector<std::pair<std::string, std::string>> ret;
      std::string name;
      bool inValue = false;
      int prevPos = 0;
      int i;
      for (i = 0; i < (int)query.length(); ++i)
      {
          if (query[i] == '=' && !inValue) {
              name = query.substr(prevPos, i - prevPos);
              prevPos = i + 1;
              inValue = true;
          }
          else if (query[i] == '&')
          {
              ret.emplace_back(
                  std::make_pair(name, unquote(query.substr(prevPos, i - prevPos))));
              name = "";
              prevPos = i + 1;
              inValue = false;
          }
      }
      if (!name.empty())
      {
          ret.emplace_back(
              std::make_pair(name, unquote(query.substr(prevPos, i - prevPos))));
      }
      return ret;
  }

  std::string AuthWebServer::getToken()
  {
      return m_saml_token;
  }

  bool AuthWebServer::isConsentCacheIdToken()
  {
      return m_consent_cache_id_token;
  }

  AuthenticatorTest::AuthenticatorTest(SF_CONNECT* conn) : m_connection(conn)
  {
  }

  AuthenticatorTest::~AuthenticatorTest()
  {
      //nop
  }

  void AuthenticatorTest::authenticate()
  {
      //nop
  }

  void AuthenticatorTest::updateDataMap(jsonObject_t& dataMap) 
  {
      dataMap["test"] = picojson::value(count);
      count++;
  }
} // namespace Client
} // namespace Snowflake
