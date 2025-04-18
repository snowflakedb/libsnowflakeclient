
#include "AzureAttestation.hpp"
#include "jwt/Jwt.hpp"
#include "snowflake/HttpClient.hpp"
#include "logger/SFLogger.hpp"

namespace {
  std::vector<std::string> allowedIssuers = {"https://sts.windows.net/", "https://login.microsoftonline.com/"};
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
        CXX_LOG_WARN("Azure metadata server request was not successful.");
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
        std::any_of(allowedIssuers.begin(), allowedIssuers.end(), [&issuer](const std::string& allowedIssuer) {
          return issuer.rfind(allowedIssuer, 0) == 0;
        });

      if (!isValidIssuer) {
        CXX_LOG_ERROR("Unexpected issuer in Azure JWT: %s", issuer.c_str());
        return boost::none;
      }

      return Attestation{AttestationType::AZURE, jwtStr, issuer, subject};
    }

    boost::url AzureAttestationConfig::getRequestURL() const {
      boost::urls::url url = boost::url("http://169.254.169.254/metadata/identity/oauth2/token");
      if (managedIdentity) {
        url = managedIdentity->endpoint;
        if (managedIdentity->clientId) {
          url.params().append({"client_id", managedIdentity->clientId.get()});
        }
      }

      url.params().append({"api-version", "2018-02-01"});
      url.params().append({"resource", snowflakeEntraResource});
      return url;
    }

    std::map<std::string, std::string> AzureAttestationConfig::getRequestHeaders() const {
      std::map<std::string, std::string> headers = {{"Metadata", "True"}};
      if (managedIdentity) {
        headers["X-IDENTITY-HEADER"] = managedIdentity->header;
      }
      return headers;
    }

    boost::optional<AzureAttestationConfig>
    Snowflake::Client::AzureAttestationConfig::fromConfig(const Snowflake::Client::AttestationConfig &config) {
      if (!config.snowflakeEntraResource) {
        CXX_LOG_INFO("Failed to create azure attestation config: snowflake entra resource missing");
        return boost::none;
      }
      AzureAttestationConfig azureConfig;
      azureConfig.snowflakeEntraResource = config.snowflakeEntraResource.get();
      azureConfig.managedIdentity = AzureManagedIdentityConfig::fromEnv();
      return azureConfig;
    }

    boost::optional<AzureManagedIdentityConfig> Snowflake::Client::AzureManagedIdentityConfig::fromEnv() {
      auto header = std::getenv("IDENTITY_HEADER");
      auto endpoint = std::getenv("IDENTITY_ENDPOINT");
      auto clientId = std::getenv("MANAGED_IDENTITY_CLIENT_ID");
      if (!header || !endpoint) {
        return boost::none;
      }

      auto parsedEndpointResult = boost::urls::parse_uri(endpoint);
      if (!parsedEndpointResult) {
        return boost::none;
      }

      AzureManagedIdentityConfig config;
      config.header = header;
      config.endpoint = parsedEndpointResult.value();
      config.clientId = clientId ? boost::optional<std::string>{clientId} : boost::none;
      return config;
    }
  }
}
