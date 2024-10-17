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

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/err.h>

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

    return AUTH_UNSUPPORTED;
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
    if (!data)
    {
      data = snowflake_cJSON_CreateObject();
      snowflake_cJSON_AddItemToObject(dataMap, "data", data);
    }
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
} // namespace Client
} // namespace Snowflake
