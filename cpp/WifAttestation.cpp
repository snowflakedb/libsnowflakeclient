#include "snowflake/AWSUtils.hpp"
#include "snowflake/WifAttestation.hpp"
#include "GcpAttestation.hpp"
#include "AzureAttestation.hpp"
#include "AwsAttestation.hpp"
#include "OIDCAttestation.hpp"
#include "logger/SFLogger.hpp"
#include "jwt/Jwt.hpp"
#include "error.h"
#include <boost/url.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace Snowflake {
  namespace Client {
    namespace {

      using AttestationProvider = std::function<boost::optional<Attestation>(AttestationConfig&)>;
      const std::map<AttestationType, AttestationProvider> attestationProviders = {
          {AttestationType::AWS, createAwsAttestation},
          {AttestationType::AZURE, createAzureAttestation},
          {AttestationType::GCP, createGcpAttestation},
          {AttestationType::OIDC, createOIDCAttestation},
      };

      bool hasScheme(const std::string& value) {
        return value.find("://") != std::string::npos;
      }

      std::string stripTrailingSlash(std::string value) {
        while (!value.empty() && value.back() == '/') {
          value.pop_back();
        }
        return value;
      }
    }

    std::string AttestationConfig::getWifHostForAws() const {
      const std::string host = getWifHost();
      if (host.empty() || !hasScheme(host)) 
      {
        return stripTrailingSlash(host);
      }

      auto parsed = boost::urls::parse_uri(host);
      if (!parsed) 
      {
        log_error("Invalid WIF host URL for AWS: %s", host.c_str());
        return host;
      }

      std::string bareHost(parsed->host());
      if (parsed->has_port()) 
      {
        bareHost += ":" + std::string(parsed->port());
      }
      return bareHost;
    }

    std::string AttestationConfig::getWifHostForGcp() const {
      const std::string host = getWifHost();
      if (host.empty()) 
      {
        return host;
      }
      if (hasScheme(host)) 
      {
        return stripTrailingSlash(host);
      }

      return "https://" + host + "/v1";
    }

    SF_STATUS AttestationConfig::configureWIFAttestation(SF_CONNECT* conn)
    {
        // Populate config from SF_CONNECT fields
        if (conn->wif_provider) 
        {
            auto typeOpt = attestationTypeFromString(conn->wif_provider);
            if (typeOpt) 
            {
                type = typeOpt;
                log_debug("Using explicit WIF provider: %s", conn->wif_provider);
            }
            else 
            {
                const char* errorMessage =
                    "Invalid WIF provider. Valid values: AWS, AZURE, GCP, OIDC";
                log_error("%s. Got: '%s'", errorMessage, conn->wif_provider);
                SET_SNOWFLAKE_ERROR(&conn->error,
                                    SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
                                    errorMessage,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
                return SF_STATUS_ERROR_BAD_CONNECTION_PARAMS;
            }
        }
        else 
        {
            const char* errorMessage =
                "WIF provider is required but not specified";
            log_error("%s", errorMessage);
            SET_SNOWFLAKE_ERROR(&conn->error,
                                SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
                                errorMessage,
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            return SF_STATUS_ERROR_BAD_CONNECTION_PARAMS;
        }

        if (conn->wif_token) 
        {
            token = std::string(conn->wif_token);
            log_debug("Using explicit WIF token");
        }

        if (conn->wif_azure_resource) 
        {
            snowflakeEntraResource = std::string(conn->wif_azure_resource);
            log_debug("Using Azure resource: %s", conn->wif_azure_resource);
        }

        // Pass workload identity impersonation path
        if (conn->workload_identity_impersonation_path)
        {
            workloadIdentityImpersonationPath = conn->workload_identity_impersonation_path;
        }

        if (conn->wif_host)
        {
            wifHost = std::string(conn->wif_host);
            log_debug("Using explicit WIF host: %s", conn->wif_host);
        }

        awsUseOutboundToken = (conn->wif_aws_use_outbound_token == SF_BOOLEAN_TRUE);

        return SF_STATUS_SUCCESS;
    }

    namespace {
      // --- WORKLOAD_IDENTITY host allowlist -------------------------------
      //
      // Suffix-anchored allowlist that restricts Workload Identity
      // attestation to recognized Snowflake hosts before any cloud
      // credential is fetched. Matching is plain string operations (no
      // regex) and is anchored to a label boundary at the end of the host,
      // so only the listed suffixes and their subdomains are recognized
      // (e.g. "evilsnowflakecomputing.com" or
      // "evil.snowflakecomputing.com.untrusted.example" are not matches).

      const char* const kDefaultAllowedHostSuffixes[] = {
          "snowflakecomputing.com",
          "snowflakecomputing.cn",
          "snowflakecomputing.mil",
      };

      const char* const kWifAllowedHostSuffixesEnvVar = "SNOWFLAKE_WIF_ALLOWED_HOST_SUFFIXES";

      std::string trim(const std::string& s) {
        size_t begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
          return std::string();
        }
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(begin, end - begin + 1);
      }

      std::string toLowerAscii(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
      }

      // Splits a comma-separated list into normalized (trimmed, lowercased,
      // trailing-dot-stripped) entries. Empty entries are ignored.
      std::vector<std::string> splitCsvNormalized(const std::string& csv) {
        std::vector<std::string> result;
        std::stringstream ss(csv);
        std::string item;
        while (std::getline(ss, item, ',')) {
          std::string normalized = toLowerAscii(trim(item));
          if (!normalized.empty() && normalized.back() == '.') {
            normalized.pop_back();
          }
          if (!normalized.empty()) {
            result.push_back(normalized);
          }
        }
        return result;
      }

      // Additive, comma-separated extra suffixes from
      // SNOWFLAKE_WIF_ALLOWED_HOST_SUFFIXES. Read only from the process
      // environment - never from the DSN, connection parameters or
      // configuration files - so connection configuration cannot influence
      // the allowlist. Entries are additive: they extend the
      // recognized-host list and cannot disable it.
      std::vector<std::string> envAllowedHostSuffixes() {
        const char* envVal = std::getenv(kWifAllowedHostSuffixesEnvVar);
        if (envVal == nullptr) {
          return {};
        }
        std::vector<std::string> extra = splitCsvNormalized(envVal);
        if (!extra.empty()) {
          std::string joined;
          for (size_t i = 0; i < extra.size(); ++i) {
            if (i > 0) joined += ", ";
            joined += extra[i];
          }
          CXX_LOG_INFO("WORKLOAD_IDENTITY host allowlist extended via %s with additional "
                        "suffixes: %s. This is additive only and does not disable the "
                        "default Snowflake host allowlist.",
                        kWifAllowedHostSuffixesEnvVar, joined.c_str());
        }
        return extra;
      }

      // True iff `host` equals `suffix` or ends with "." + suffix, i.e. the
      // suffix match is anchored on a label boundary. Both inputs are
      // assumed already normalized (lowercase, no trailing dot).
      bool hostMatchesSuffix(const std::string& host, const std::string& suffix) {
        if (suffix.empty()) {
          return false;
        }
        if (host == suffix) {
          return true;
        }
        const std::string dotSuffix = "." + suffix;
        if (host.size() <= dotSuffix.size()) {
          return false;
        }
        return host.compare(host.size() - dotSuffix.size(), dotSuffix.size(), dotSuffix) == 0;
      }
    }

    bool isSnowflakeHostForWorkloadIdentity(const std::string& host) {
      std::string h = toLowerAscii(trim(host));

      // Defensive :port strip. Must happen before the trailing-dot strip
      // below so that an FQDN-with-port host like
      // "acct.snowflakecomputing.com.:443" ends up as
      // "acct.snowflakecomputing.com." (dot still attached) rather than
      // being left as-is with the port never stripped. IPv6 literals (which
      // also contain ':') never match a Snowflake suffix below, so no
      // special-casing is needed for them; they are rejected implicitly.
      size_t colonPos = h.find(':');
      if (colonPos != std::string::npos) {
        h = h.substr(0, colonPos);
      }

      if (!h.empty() && h.back() == '.') {
        h.pop_back();
      }

      if (h.empty()) {
        CXX_LOG_ERROR("WORKLOAD_IDENTITY authenticator rejected: empty host");
        return false;
      }

      for (const char* suffix : kDefaultAllowedHostSuffixes) {
        if (hostMatchesSuffix(h, suffix)) {
          return true;
        }
      }
      for (const std::string& suffix : envAllowedHostSuffixes()) {
        if (hostMatchesSuffix(h, suffix)) {
          return true;
        }
      }

      CXX_LOG_ERROR("WORKLOAD_IDENTITY requires a recognized Snowflake host "
                    "(*.snowflakecomputing.com, .cn or .mil). Got: '%s'",
                    h.c_str());
      return false;
    }

    boost::optional<Attestation> createAttestation(AttestationConfig& config) {
      if (config.httpClient == NULL)
        config.httpClient = IHttpClient::getInstance();
      if (config.awsSdkWrapper == NULL)
        config.awsSdkWrapper = AwsUtils::ISdkWrapper::getInstance();

      if (!config.type) {
        CXX_LOG_ERROR("Attestation type must be specified");
        return boost::none;
      }

      auto type = config.type.get();
      return attestationProviders.at(type)(config);
    }
  }
}
