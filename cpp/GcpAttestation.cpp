#include "GcpAttestation.hpp"
#include "jwt/Jwt.hpp"
#include "snowflake/HttpClient.hpp"
#include "logger/SFLogger.hpp"
#include <picojson.h>
#include <boost/url.hpp>
#include <sstream>

namespace Snowflake::Client {
  constexpr auto SNOWFLAKE_AUDIENCE = "snowflakecomputing.com";
  constexpr auto GCP_METADATA_SERVER_BASE_URL = "http://169.254.169.254/computeMetadata/v1/instance/service-accounts/default/";
  constexpr auto GCP_IAM_CREDENTIALS_BASE_URL = "https://iamcredentials.googleapis.com/v1";

  // Splits comma-separated impersonation path
  std::vector<std::string> parseImpersonationPath(const std::string &path) {
    std::vector<std::string> result;
    std::stringstream ss(path);
    std::string item;

    while (std::getline(ss, item, ',')) {
      const auto start = item.find_first_not_of(" \t");
      const auto end = item.find_last_not_of(" \t");

      if (start != std::string::npos) {
        result.push_back(item.substr(start, end - start + 1));
      }
    }

    return result;
  }

  // Fetches access token from metadata server
  boost::optional<std::string> getGcpAccessToken(IHttpClient *httpClient) {
    const auto url = boost::urls::url(std::string(GCP_METADATA_SERVER_BASE_URL) + "token");

    const HttpRequest req{
      HttpRequest::Method::GET,
      url,
      {
        {"Metadata-Flavor", "Google"},
      }
    };

    auto responseOpt = httpClient->run(req);
    if (!responseOpt) {
      CXX_LOG_INFO("No response from GCP metadata server for access token.");
      return boost::none;
    }

    const auto &response = responseOpt.get();
    if (response.code != 200) {
      CXX_LOG_ERROR("GCP metadata server access token request was not successful. Code: %ld", response.code);
      return boost::none;
    }

    const std::string response_body = response.getBody();
    picojson::value json;
    const std::string err = picojson::parse(json, response_body);
    if (!err.empty()) {
      CXX_LOG_ERROR("Error parsing GCP access token response: %s", err.c_str());
      return boost::none;
    }

    if (!json.is<picojson::object>() || !json.get("access_token").is<std::string>()) {
      CXX_LOG_ERROR("No access_token found in GCP response.");
      return boost::none;
    }

    return json.get("access_token").get<std::string>();
  }

  // Fetches identity token using delegation chain
  boost::optional<std::string> getIdentityTokenWithDelegation(
    IHttpClient *httpClient,
    const std::string &accessToken,
    const std::vector<std::string> &serviceAccountChain) {
    if (serviceAccountChain.empty()) {
      CXX_LOG_ERROR("Service account chain is empty");
      return boost::none;
    }

    const std::string &targetServiceAccount = serviceAccountChain.back();

    const std::vector<std::string> delegates(
      serviceAccountChain.begin(),
      serviceAccountChain.end() - 1
    );

    std::string idTokenUrl = std::string(GCP_IAM_CREDENTIALS_BASE_URL)
                             + "/projects/-/serviceAccounts/"
                             + targetServiceAccount + ":generateIdToken";

    picojson::object requestBody;
    requestBody["audience"] = picojson::value(std::string(SNOWFLAKE_AUDIENCE));
    requestBody["includeEmail"] = picojson::value(true);

    if (!delegates.empty()) {
      picojson::array delegatesArray;
      for (const auto &delegate: delegates) {
        // Format: projects/-/serviceAccounts/{email}
        std::string delegateStr = "projects/-/serviceAccounts/" + delegate;
        delegatesArray.emplace_back(delegateStr);
      }
      requestBody["delegates"] = picojson::value(delegatesArray);
    }

    std::string requestBodyStr = picojson::value(requestBody).serialize();
    CXX_LOG_DEBUG("GCP generateIdToken request body: %s", requestBodyStr.c_str());

    auto url = boost::urls::parse_uri(idTokenUrl);
    if (!url) {
      CXX_LOG_ERROR("Invalid ID token URL: %s", idTokenUrl.c_str());
      return boost::none;
    }

    HttpRequest req{
      HttpRequest::Method::POST,
      url.value(),
      {
        {"Authorization", "Bearer " + accessToken},
        {"Content-Type", "application/json"},
      },
      requestBodyStr
    };

    auto responseOpt = httpClient->run(req);
    if (!responseOpt) {
      CXX_LOG_ERROR("No response from GCP generateIdToken API.");
      return boost::none;
    }

    const auto &response = responseOpt.get();
    if (response.code != 200) {
      CXX_LOG_ERROR("GCP generateIdToken API request failed with code %ld: %s",
                    response.code, response.getBody().c_str());
      return boost::none;
    }

    std::string response_body = response.getBody();
    picojson::value json;
    std::string err = picojson::parse(json, response_body);
    if (!err.empty()) {
      CXX_LOG_ERROR("Error parsing GCP ID token response: %s", err.c_str());
      return boost::none;
    }

    if (!json.is<picojson::object>() || !json.get("token").is<std::string>()) {
      CXX_LOG_ERROR("No token found in ID token response.");
      return boost::none;
    }

    return json.get("token").get<std::string>();
  }

  boost::optional<Attestation> createGcpAttestation(AttestationConfig &config) {
    std::string jwtStr;

    // Check if service account impersonation is configured
    if (config.workloadIdentityImpersonationPath &&
        !config.workloadIdentityImpersonationPath.get().empty()) {
      CXX_LOG_INFO("Using GCP service account impersonation with delegation");

      auto serviceAccountChain = parseImpersonationPath(
        config.workloadIdentityImpersonationPath.get());

      if (serviceAccountChain.empty()) {
        CXX_LOG_ERROR("Failed to parse service account impersonation path");
        return boost::none;
      }

      CXX_LOG_DEBUG("Service account chain size: %zu", serviceAccountChain.size());

      // Get access token from metadata server
      auto accessTokenOpt = getGcpAccessToken(config.httpClient);
      if (!accessTokenOpt) {
        CXX_LOG_ERROR("Failed to get access token from metadata server");
        return boost::none;
      }

      // Get identity token from IAM Credentials API with delegation
      auto idTokenOpt = getIdentityTokenWithDelegation(
        config.httpClient,
        accessTokenOpt.get(),
        serviceAccountChain);
      if (!idTokenOpt) {
        CXX_LOG_ERROR("Failed to get identity token with delegation");
        return boost::none;
      }

      jwtStr = idTokenOpt.get();
    } else {
      // Get identity token directly from metadata server
      CXX_LOG_INFO("Using direct GCP identity token from metadata server");

      auto url = boost::urls::url(std::string(GCP_METADATA_SERVER_BASE_URL) + "identity");
      url.params().append({"audience", SNOWFLAKE_AUDIENCE});

      HttpRequest req{
        HttpRequest::Method::GET,
        url,
        {
          {"Metadata-Flavor", "Google"},
        }
      };

      auto responseOpt = config.httpClient->run(req);
      if (!responseOpt) {
        CXX_LOG_INFO("No response from GCP metadata server.");
        return boost::none;
      }

      const auto &response = responseOpt.get();
      if (response.code != 200) {
        CXX_LOG_ERROR("GCP metadata server request was not successful.");
        return boost::none;
      }

      jwtStr = response.getBody();
      if (jwtStr.empty()) {
        CXX_LOG_ERROR("No JWT found in GCP response.");
        return boost::none;
      }
    }

    // Parse JWT and extract issuer/subject
    Jwt::JWTObject jwt(jwtStr);
    auto claimSet = jwt.getClaimSet();
    std::string issuer = claimSet->getClaimInString("iss");
    std::string subject = claimSet->getClaimInString("sub");
    if (issuer.empty() || subject.empty()) {
      CXX_LOG_ERROR("No issuer or subject found in GCP JWT.");
      return boost::none;
    }

    return Attestation::makeGcp(jwtStr, issuer, subject);
  }
}
