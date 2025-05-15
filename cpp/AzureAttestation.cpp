#include <picojson.h>
#include "AzureAttestation.hpp"
#include "jwt/Jwt.hpp"
#include "snowflake/HttpClient.hpp"
#include "logger/SFLogger.hpp"

namespace {
  const std::vector<std::string> allowedIssuerPrefixes = {"https://sts.windows.net/", "https://login.microsoftonline.com/"};
  const std::string defaultSnowflakeEntraResource = "api://fd3f753b-eed3-462c-b6a7-a4b5bb650aad";
}

namespace Snowflake {
  namespace Client {
    boost::optional<Attestation> createAzureAttestation(AttestationConfig& config) {
      auto azureConfigOpt = AzureAttestationConfig::fromConfig(config);
      if (!azureConfigOpt) {
        return boost::none;
      }

      const auto& azureConfig = azureConfigOpt.get();
      HttpRequest req{
          HttpRequest::Method::GET,
          azureConfig.getRequestURL(),
          azureConfig.getRequestHeaders()
      };

      auto responseOpt = config.httpClient->run(req);
      if (!responseOpt) {
        CXX_LOG_ERROR("No response from Azure metadata server.");
        return boost::none;
      }

      const auto& response = responseOpt.get();
      if (response.code != 200) {
        CXX_LOG_ERROR("Azure metadata server request was not successful.");
        return boost::none;
      }

      std::string response_body = response.getBody();
      picojson::value json;
      std::string err = picojson::parse(json, response_body);
      if (!err.empty()) {
        CXX_LOG_ERROR("Error parsing Azure response: %s", err.c_str());
        return boost::none;
      }

      std::string jwtStr = json.get("access_token").get<std::string>();
      if (jwtStr.empty()) {
        CXX_LOG_ERROR("No access token found in Azure response.");
        return boost::none;
      }

      Jwt::JWTObject jwt(jwtStr);
      auto claimSet = jwt.getClaimSet();
      std::string issuer = claimSet->getClaimInString("iss");
      std::string subject = claimSet->getClaimInString("sub");
      if (issuer.empty() || subject.empty()) {
        CXX_LOG_ERROR("No issuer or subject found in Azure JWT.");
        return boost::none;
      }

      bool isValidIssuer =
        std::any_of(allowedIssuerPrefixes.cbegin(), allowedIssuerPrefixes.cend(), [&issuer](const std::string& allowedIssuer) {
          return issuer.rfind(allowedIssuer, 0) == 0;
        });

      if (!isValidIssuer) {
        CXX_LOG_ERROR("Unexpected issuer in Azure JWT: %s", issuer.c_str());
        return boost::none;
      }

      return Attestation::makeAzure(jwtStr, issuer, subject);
    }

    boost::url AzureAttestationConfig::getRequestURL() const {
      boost::urls::url url = boost::url("http://169.254.169.254/metadata/identity/oauth2/token");
      if (managedIdentity) {
        url = managedIdentity->endpoint;
        if (managedIdentity->clientId) {
          url.params().append({"client_id", managedIdentity->clientId.get()});
        }
        url.params().append({"api-version", "2019-08-01"});
      }
      else {
        url.params().append({"api-version", "2018-02-01"});
      }

      url.params().append({"resource", snowflakeEntraResource});
      return url;
    }

    std::map<std::string, std::string> AzureAttestationConfig::getRequestHeaders() const {
      if (managedIdentity) {
        return {{"X-IDENTITY-HEADER", managedIdentity->header}};
      }
      else {
        return {{"Metadata", "True"}};
      }
    }

    boost::optional<AzureAttestationConfig>
    Snowflake::Client::AzureAttestationConfig::fromConfig(const Snowflake::Client::AttestationConfig &config) {
      AzureAttestationConfig azureConfig;
      azureConfig.snowflakeEntraResource = config.snowflakeEntraResource.get_value_or(defaultSnowflakeEntraResource);
      azureConfig.managedIdentity = AzureFunctionsManagedIdentityConfig::fromEnv();
      return azureConfig;
    }

    boost::optional<AzureFunctionsManagedIdentityConfig> Snowflake::Client::AzureFunctionsManagedIdentityConfig::fromEnv() {
      auto header = std::getenv("IDENTITY_HEADER");
      auto endpoint = std::getenv("IDENTITY_ENDPOINT");
      auto clientId = std::getenv("MANAGED_IDENTITY_CLIENT_ID");
      if (!header || !endpoint) {
        return boost::none;
      }

      auto parsedEndpointResult = boost::urls::parse_uri(endpoint);
      if (!parsedEndpointResult) {
        CXX_LOG_INFO("Failed to parse IDENTITY_ENDPOINT: %s", parsedEndpointResult.error().message().c_str());
        return boost::none;
      }

      AzureFunctionsManagedIdentityConfig config;
      config.header = header;
      config.endpoint = parsedEndpointResult.value();
      config.clientId = clientId ? boost::optional<std::string>{clientId} : boost::none;
      return config;
    }
  }
}
