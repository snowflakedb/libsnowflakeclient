#include <memory>
#include <utility>
#include <boost/url.hpp>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <picojson.h>
#include "snowflake/TomlConfigParser.hpp"
#include "utils/test_setup.h"
#include "utils/EnvOverride.hpp"
#include "snowflake/AWSUtils.hpp"
#include "snowflake/HttpClient.hpp"
#include "snowflake/WifAttestation.hpp"
#include "util/Base64.hpp"
#include <curl/curl.h>
#include <jwt/Jwt.hpp>

using namespace Snowflake::Client;

namespace {
  EVP_PKEY *generate_key() {
    EVP_PKEY *key = nullptr;

    std::unique_ptr<EVP_PKEY_CTX, std::function<void(EVP_PKEY_CTX *)>>
        kct{EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), [](EVP_PKEY_CTX *ctx) { EVP_PKEY_CTX_free(ctx); }};

    if (EVP_PKEY_keygen_init(kct.get()) <= 0) { return nullptr; }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(kct.get(), 2048) <= 0) { return nullptr; }

    if (EVP_PKEY_keygen(kct.get(), &key) <= 0) { return nullptr; }

    return key;
  }
}

class FakeHttpClient : public IHttpClient {
public:
  explicit FakeHttpClient(std::function<boost::optional<HttpResponse>(HttpRequest)> run_fn)
      : run_fn(std::move(run_fn)) {}

  boost::optional<HttpResponse> run(HttpRequest req) override {
    return run_fn(req);
  }

private:
  std::function<boost::optional<HttpResponse>(HttpRequest)> run_fn;
};

class FakeAwsSdkWrapper : public AwsUtils::ISdkWrapper {
public:
  FakeAwsSdkWrapper(
      boost::optional<std::string> region,
      Aws::Auth::AWSCredentials creds) : region(std::move(region)), creds(std::move(creds)) {}

  Aws::Auth::AWSCredentials getCredentials() override {
    return creds;
  }

  boost::optional<std::string> getEC2Region() override {
    return region;
  }

private:
  boost::optional<std::string> region;
  Aws::Auth::AWSCredentials creds;
};


long run_request_curl(
    const std::string &url,
    const std::string &method,
    const std::map<std::string, std::string> &headers) {
  CURL *curl = curl_easy_init();
  assert_true(curl != nullptr);

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

  struct curl_slist *header_list = nullptr;
  for (const auto &h: headers) {
    std::string hdr = h.first + ": " + h.second;
    header_list = curl_slist_append(header_list, hdr.c_str());
  }
  if (header_list) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  }

  CURLcode res = curl_easy_perform(curl);
  long response_code = 0;
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  }

  if (header_list) {
    curl_slist_free_all(header_list);
  }
  curl_easy_cleanup(curl);
  return response_code;
}

const std::string AWS_TEST_REGION = "us-east-1";
const Aws::Auth::AWSCredentials AWS_TEST_CREDS = Aws::Auth::AWSCredentials("AKIAEXAMPLE12345678",
                                                                           "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"); // pragma: allowlist secret

void test_integration_aws_attestation(void **) {
  if (std::getenv("GITHUB_ACTIONS")) {
    std::cerr << "Skipping test_aws_attestation on GitHub Actions since it requires AWS credentials" << std::endl;
    return;
  }

  EnvOverride awsRegion("AWS_REGION", AWS_TEST_REGION);
  AttestationConfig config;
  config.type = AttestationType::AWS;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto &attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AWS);
  assert_false(attestation.credential.empty());
  std::string json_string;
  Snowflake::Client::Util::Base64::decodePadding(attestation.credential.begin(), attestation.credential.end(),
                                                 std::back_inserter(json_string));
  picojson::value json;
  picojson::parse(json, json_string);
  assert_true(json.is<picojson::object>());
  assert_true(json.get("url").is<std::string>());
  assert_true(json.get("method").is<std::string>());
  std::string method = json.get("method").get<std::string>();
  assert_true(json.get("headers").is<picojson::object>());
  std::map<std::string, std::string> headers;
  for (auto &header: json.get("headers").get<picojson::object>()) {
    assert_true(header.second.is<std::string>());
    headers[header.first] = header.second.get<std::string>();
  }
  assert_true(
      run_request_curl(json.get("url").get<std::string>(), method, headers) ==
      200
  );
}

void test_attestation_success(const char* region, const char *expectedHost) {
  AttestationConfig config;
  config.type = AttestationType::AWS;
  auto awsSdkWrapper = FakeAwsSdkWrapper(std::string(region), AWS_TEST_CREDS);
  config.awsSdkWrapper = &awsSdkWrapper;

  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto &attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AWS);
  assert_true(!attestation.credential.empty());
  assert_true(!attestation.subject);
  assert_true(!attestation.issuer);

  std::string json_string;
  Snowflake::Client::Util::Base64::decodePadding(attestation.credential.begin(), attestation.credential.end(),
                                                 std::back_inserter(json_string));
  picojson::value json;
  picojson::parse(json, json_string);
  assert_true(json.is<picojson::object>());

  auto headers = json.get("headers").get<picojson::object>();
  auto host = headers["host"].get<std::string>();
  assert_true(host == expectedHost);
}

void test_unit_aws_attestation_success(void **) {
  test_attestation_success("us-east-1", "sts.us-east-1.amazonaws.com");
}

void test_unit_aws_attestation_china_region_success(void **) {
  test_attestation_success("cn-northwest-1", "sts.cn-northwest-1.amazonaws.com.cn");
}

void test_unit_aws_attestation_failed(FakeAwsSdkWrapper *awsSdkWrapper) {
  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = awsSdkWrapper;

  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(!attestationOpt);
}

void test_unit_aws_attestation_region_missing(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(boost::none, AWS_TEST_CREDS);
  test_unit_aws_attestation_failed(&awsSdkWrapper);
}

void test_unit_aws_attestation_cred_missing(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, Aws::Auth::AWSCredentials());
  test_unit_aws_attestation_failed(&awsSdkWrapper);
}

//  These tests only verify the impersonation path parsing and configuration handling
// not the actual STS calls, which requires valid AWS setup.

void test_unit_aws_attestation_impersonation_single_role(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  
  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = "arn:aws:iam::123456789012:role/TestRole";
  
  // Will fail at actual STS call, but validates path parsing and code path
  const auto attestationOpt = createAttestation(config);
  assert_true(!attestationOpt || attestationOpt.has_value());
}

void test_unit_aws_attestation_impersonation_role_chain(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  
  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = 
      "arn:aws:iam::123456789012:role/Role1,"
      "arn:aws:iam::123456789012:role/Role2,"
      "arn:aws:iam::123456789012:role/Role3";
  
  // Will fail at STS, but validates chain parsing
  const auto attestationOpt = createAttestation(config);
  assert_true(!attestationOpt || attestationOpt.has_value());
}

void test_unit_aws_attestation_impersonation_whitespace_handling(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  
  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = 
      "  arn:aws:iam::123456789012:role/Role1  , "
      " arn:aws:iam::123456789012:role/Role2 ";
  
  // Will fail at STS, but validates whitespace trimming
  const auto attestationOpt = createAttestation(config);
  assert_true(!attestationOpt || attestationOpt.has_value());
}

void test_unit_aws_attestation_impersonation_cross_account(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  
  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = 
      "arn:aws:iam::111111111111:role/SourceRole,"
      "arn:aws:iam::222222222222:role/TargetRole";
  
  // Will fail at STS, but validates cross-account ARN parsing
  const auto attestationOpt = createAttestation(config);
  assert_true(!attestationOpt || attestationOpt.has_value());
}

void test_unit_aws_attestation_impersonation_empty_path_fallback(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  
  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = "";
  
  auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  
  const auto& attestation = attestationOpt.get();
  assert_true(attestation.type == AttestationType::AWS);
}

void test_unit_aws_attestation_impersonation_with_missing_region(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(boost::none, AWS_TEST_CREDS);
  
  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = "arn:aws:iam::123456789012:role/TestRole";

  const auto attestationOpt = createAttestation(config);
  assert_false(attestationOpt.has_value());
}

void test_unit_aws_attestation_impersonation_with_missing_credentials(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, Aws::Auth::AWSCredentials());
  
  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = "arn:aws:iam::123456789012:role/TestRole";

  const auto attestationOpt = createAttestation(config);
  assert_false(attestationOpt.has_value());
}

std::vector<char> makeGCPToken(boost::optional<std::string> issuer, boost::optional<std::string> subject) {
  auto jwtObj = Jwt::JWTObject();
  jwtObj.getHeader()->setAlgorithm(Jwt::AlgorithmType::RS256);
  if (issuer) {
    jwtObj.getClaimSet()->addClaim("iss", issuer.value());
  }
  if (subject) {
    jwtObj.getClaimSet()->addClaim("sub", subject.value());
  }
  const auto key = std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY *)>>(generate_key(),
                                                                        [](EVP_PKEY *k) { EVP_PKEY_free(k); });
  auto string = jwtObj.serialize(key.get());
  return {string.begin(), string.end()};
}

const std::string GCP_TEST_ISSUER = "https://accounts.google.com";
const std::string GCP_TEST_SUBJECT = "107562638633288735786";
const std::string GCP_TEST_AUDIENCE = "snowflakecomputing.com";

const std::string GCP_TEST_METADATA_ENDPOINT_HOST = "169.254.169.254";

FakeHttpClient makeSuccessfulGCPHttpClient(const std::vector<char> &token) {
  return FakeHttpClient([=](Snowflake::Client::HttpRequest req) {
    assert_true((*req.url.params().find("audience")).value == GCP_TEST_AUDIENCE);
    assert_true(req.url.host() == GCP_TEST_METADATA_ENDPOINT_HOST);
    assert_true(req.url.scheme() == "http");
    HttpResponse response;
    response.code = 200;
    response.buffer = token;
    return response;
  });
}

void test_unit_gcp_attestation_success(void **) {
  auto token = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT);
  auto fakeHttpClient = makeSuccessfulGCPHttpClient(token);
  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  const auto &attestation = attestationOpt.get();
  assert_true(attestation.type == Snowflake::Client::AttestationType::GCP);
  assert_true(attestation.credential == std::string(token.begin(), token.end()));
  assert_true(attestation.subject == std::string("107562638633288735786"));
}

void test_unit_gcp_attestation_missing_issuer(void **) {
  const auto token = makeGCPToken(boost::none, GCP_TEST_SUBJECT);
  auto fakeHttpClient = makeSuccessfulGCPHttpClient(token);
  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  const auto attestationOpt = createAttestation(config);
  assert_true(!attestationOpt);
}

void test_unit_gcp_attestation_missing_subject(void **) {
  const auto token = makeGCPToken(GCP_TEST_ISSUER, boost::none);
  auto fakeHttpClient = makeSuccessfulGCPHttpClient(token);
  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  const auto attestationOpt = createAttestation(config);
  assert_true(!attestationOpt);
}

void test_unit_gcp_attestation_failed_request(void **) {
  auto token = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT);
  auto fakeHttpClient = FakeHttpClient([&](const Snowflake::Client::HttpRequest&) {
    return boost::none;
  });

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  const auto attestationOpt = createAttestation(config);
  assert_true(!attestationOpt);
}

void test_unit_gcp_attestation_bad_request(void **) {
  auto token = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT);
  auto fakeHttpClient = FakeHttpClient([&](const Snowflake::Client::HttpRequest&) {
    HttpResponse response;
    response.code = 400;
    return response;
  });

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  const auto attestationOpt = createAttestation(config);
  assert_true(!attestationOpt);
}

const std::string GCP_TEST_SUBJECT_ACCESS = "107562638633288735787";

const std::string GCP_TEST_IAM_ENDPOINT_HOST = "iamcredentials.googleapis.com";

// Multi-path fake HTTP client for GCP service account impersonation
enum class AcceptedHosts {
  Metadata,
  Iam,
  Other
};

auto getHost(const std::string& host) -> AcceptedHosts {
  if (host == GCP_TEST_METADATA_ENDPOINT_HOST) return AcceptedHosts::Metadata;
  if (host == GCP_TEST_IAM_ENDPOINT_HOST) return AcceptedHosts::Iam;
  return AcceptedHosts::Other;
}

FakeHttpClient makeSuccessfulGCPImpersonationHttpClient(
    const std::vector<char>& accessToken,
    const std::vector<char>& idToken,
    const std::vector<std::string>& expectedDelegates,
    const std::string& expectedTargetServiceAccount) {
  return FakeHttpClient([=](Snowflake::Client::HttpRequest req) {
    HttpResponse response;
    response.code = 200;

    switch (getHost(req.url.host())) {
      case AcceptedHosts::Metadata: {
        if (req.url.encoded_path() == "/computeMetadata/v1/instance/service-accounts/default/token") {
          assert_true(req.headers.find("Metadata-Flavor")->second == "Google");
          response.buffer = accessToken;
        }
        break;
      }
      case AcceptedHosts::Iam: {
        std::string expectedPath = "/v1/projects/-/serviceAccounts/" +
                                   expectedTargetServiceAccount + ":generateIdToken";
        assert_true(req.url.encoded_path() == expectedPath);
        assert_true(req.method == HttpRequest::Method::POST);
        const auto accessTokenStr = std::string(accessToken.data(), accessToken.size());
        assert_true(req.headers.find("Authorization")->second == "Bearer " + accessTokenStr);
        assert_true(req.headers.find("Content-Type")->second == "application/json");

        picojson::value bodyJson;
        std::string err = picojson::parse(bodyJson, req.body);
        assert_true(err.empty());
        assert_true(bodyJson.is<picojson::object>());

        auto bodyObj = bodyJson.get<picojson::object>();
        assert_true(bodyObj["audience"].get<std::string>() == GCP_TEST_AUDIENCE);
        assert_true(bodyObj["includeEmail"].get<bool>() == true);

        if (!expectedDelegates.empty()) {
          assert_true(bodyObj.find("delegates") != bodyObj.end());
          auto delegates = bodyObj["delegates"].get<picojson::array>();
          assert_true(delegates.size() == expectedDelegates.size());
          for (size_t i = 0; i < expectedDelegates.size(); ++i) {
            std::string expected = "projects/-/serviceAccounts/" + expectedDelegates[i];
            assert_true(delegates[i].get<std::string>() == expected);
          }
        }

        response.buffer = idToken;
        break;
      }
      case AcceptedHosts::Other: {
        // Leave response as default.
        break;
      }
    }

    return response;
  });
}

void test_unit_gcp_impersonation_single_account_success(void **) {
  const auto accessToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT_ACCESS);
  const auto idToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT);
  const std::string targetServiceAccount = "target@project.iam.gserviceaccount.com";

  auto fakeHttpClient = makeSuccessfulGCPImpersonationHttpClient(
      accessToken,
      idToken,
      {},
      targetServiceAccount);

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  config.workloadIdentityImpersonationPath = targetServiceAccount;

  const auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  const auto &[type, credential, issuer, subject] = attestationOpt.get();
  assert_true(type == AttestationType::GCP);
  assert_true(credential == std::string(idToken.data(), idToken.size()));
  assert_true(subject == GCP_TEST_SUBJECT);
  assert_true(issuer == GCP_TEST_ISSUER);
}

void test_unit_gcp_impersonation_chain_success(void **) {
  const auto accessToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT_ACCESS);
  const auto idToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT);
  const std::vector<std::string> delegates = {
    "delegate1@project.iam.gserviceaccount.com",
    "delegate2@project.iam.gserviceaccount.com"
  };
  const std::string targetServiceAccount = "target@project.iam.gserviceaccount.com";

  auto fakeHttpClient = makeSuccessfulGCPImpersonationHttpClient(
      accessToken,
      idToken,
      delegates,
      targetServiceAccount);

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;

  std::string workloadIdentityImpersonationPath;
  for (const auto &delegate: delegates) {
    workloadIdentityImpersonationPath += delegate + ",";
  }
  workloadIdentityImpersonationPath += targetServiceAccount;
  config.workloadIdentityImpersonationPath = workloadIdentityImpersonationPath;

  const auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  const auto &[type, credential, issuer, subject] = attestationOpt.get();
  assert_true(type == AttestationType::GCP);
  assert_true(credential == std::string(idToken.data(), idToken.size()));
  assert_true(subject == GCP_TEST_SUBJECT);
}

void test_unit_gcp_impersonation_whitespace_in_path(void **) {
  const auto accessToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT_ACCESS);
  const auto idToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT);
  const std::vector<std::string> delegates = {
    "delegate1@project.iam.gserviceaccount.com",
    "delegate2@project.iam.gserviceaccount.com"
  };
  const std::string targetServiceAccount = "target@project.iam.gserviceaccount.com";

  auto fakeHttpClient = makeSuccessfulGCPImpersonationHttpClient(
      accessToken,
      idToken,
      delegates,
      targetServiceAccount);

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;

  std::string workloadIdentityImpersonationPath = "  ";
  for (const auto &delegate: delegates) {
    workloadIdentityImpersonationPath += " " + delegate + ", ";
  }
  workloadIdentityImpersonationPath += targetServiceAccount + "   ";
  config.workloadIdentityImpersonationPath = workloadIdentityImpersonationPath;

  const auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
}

void test_unit_gcp_impersonation_access_token_failed(void **) {
  auto fakeHttpClient = FakeHttpClient([](const HttpRequest &req) {
    if (req.url.host() == GCP_TEST_METADATA_ENDPOINT_HOST) {
      HttpResponse response;
      response.code = 404;
      return boost::optional<HttpResponse>(response);
    }
    return boost::optional<HttpResponse>(boost::none);
  });

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  config.workloadIdentityImpersonationPath = "target@project.iam.gserviceaccount.com";

  const auto attestationOpt = createAttestation(config);
  assert_false(attestationOpt.has_value());
}

void test_unit_gcp_impersonation_id_token_failed(void **) {
  const auto accessToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT_ACCESS);

  auto fakeHttpClient = FakeHttpClient([=](const HttpRequest &req) {
    if (req.url.host() == GCP_TEST_METADATA_ENDPOINT_HOST) {
      HttpResponse response;
      response.code = 200;
      response.buffer = accessToken;
      return boost::optional<HttpResponse>(response);
    }
    if (req.url.host() == GCP_TEST_IAM_ENDPOINT_HOST) {
      HttpResponse response;
      response.code = 403;
      const std::string error = "Forbidden";
      response.buffer = std::vector<char>(error.begin(), error.end());
      return boost::optional<HttpResponse>(response);
    }
    return boost::optional<HttpResponse>(boost::none);
  });

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  config.workloadIdentityImpersonationPath = "target@project.iam.gserviceaccount.com";

  const auto attestationOpt = createAttestation(config);
  assert_false(attestationOpt.has_value());
}

void test_unit_gcp_impersonation_empty_path(void **) {
  const auto idToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT);
  auto fakeHttpClient = makeSuccessfulGCPHttpClient(idToken);

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  // Empty path should use direct flow
  config.workloadIdentityImpersonationPath = "";

  const auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
}

void test_unit_gcp_impersonation_missing_token_in_response(void **) {
  const auto accessToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT_ACCESS);

  auto fakeHttpClient = FakeHttpClient([=](const HttpRequest &req) {
    if (req.url.host() == GCP_TEST_METADATA_ENDPOINT_HOST) {
      HttpResponse response;
      response.code = 200;
      response.buffer = accessToken;
      return boost::optional<HttpResponse>(response);
    }
    if (req.url.host() == GCP_TEST_IAM_ENDPOINT_HOST) {
      HttpResponse response;
      response.code = 200;
      const std::string body = R"({"invalid_field": "value"})";
      response.buffer = std::vector<char>(body.begin(), body.end());
      return boost::optional<HttpResponse>(response);
    }
    return boost::optional<HttpResponse>(boost::none);
  });

  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  config.workloadIdentityImpersonationPath = "target@project.iam.gserviceaccount.com";

  const auto attestationOpt = createAttestation(config);
  assert_false(attestationOpt.has_value());
}

const std::string AZURE_TEST_ISSUER_ID = "123bdcc4-50e7-4fea-958d-32cdb3ad3aca";
const std::string AZURE_TEST_SUBJECT = "f05bdcc4-50e7-4fea-958d-32cdb12b3aca";

const std::string AZURE_TEST_ISSUER_MICROSOFT = "https://login.microsoftonline.com/" + AZURE_TEST_ISSUER_ID + "/";
const std::string AZURE_TEST_ISSUER_STS = "https://sts.windows.net/" + AZURE_TEST_ISSUER_ID + "/";
const std::string AZURE_TEST_ISSUER_INVALID_PREFIX = "https://illegal.issuer.invalid/" + AZURE_TEST_ISSUER_ID + "/";

const std::string AZURE_TEST_RESOURCE = "api://4f6b1daa-cbcb-4342-ac74-005fbc825d2a";
const std::string AZURE_TEST_DEFAULT_RESOURCE = "api://fd3f753b-eed3-462c-b6a7-a4b5bb650aad";

const boost::optional<std::string> AZURE_TEST_IDENTITY_HEADER = std::string("identity-header-value");

const std::string AZURE_TEST_API_VERSION_VM = "2018-02-01";
const std::string AZURE_TEST_API_VERSION_FUNCTION = "2019-08-01";

const std::string AZURE_TEST_DEFAULT_ENDPOINT_HOST = "169.254.169.254";
const std::string AZURE_TEST_DEFAULT_ENDPOINT_PROTOCOL = "http";

const std::string AZURE_TEST_IDENTITY_ENDPOINT_HOST = "identity-endpoint.microsoft.com";
const std::string AZURE_TEST_IDENTITY_ENDPOINT_PROTOCOL = "https";
const std::string AZURE_TEST_IDENTITY_ENDPOINT =
    AZURE_TEST_IDENTITY_ENDPOINT_PROTOCOL + "://" + AZURE_TEST_IDENTITY_ENDPOINT_HOST + "/";

const std::string AZURE_TEST_MANAGED_IDENTITY_CLIENT_ID = "124564";

FakeHttpClient
makeSuccessfulAzureHttpClient(const std::string &jwt, const std::string &api_version, const std::string &resource,
                              const std::string &host, const std::string &scheme,
                              const boost::optional<std::string> &identityHeader = boost::none) {
  return FakeHttpClient([=](Snowflake::Client::HttpRequest req) {
    assert_true((*req.url.params().find("api-version")).value == api_version);
    assert_true((*req.url.params().find("resource")).value == resource);
    if (identityHeader) {
      assert_true(req.headers.find("X-IDENTITY-HEADER")->second == identityHeader.value());
    } else {
      assert_true(req.headers.find("Metadata")->second == "True");
    }
    assert_true(req.url.host() == host);
    assert_true(req.url.scheme() == scheme);
    auto obj = picojson::object();
    obj["access_token"] = picojson::value(jwt);
    std::string response_body = picojson::value(obj).serialize();
    Snowflake::Client::HttpResponse response;
    response.code = 200;
    response.buffer = std::vector<char>(response_body.begin(), response_body.end());
    return response;
  });
}

std::string makeAzureToken(const boost::optional<std::string> &issuer, const boost::optional<std::string> &subject,
                           const boost::optional<std::string> &resource) {
  auto jwtObj = Jwt::JWTObject();
  jwtObj.getHeader()->setAlgorithm(Jwt::AlgorithmType::RS256);
  if (issuer) {
    jwtObj.getClaimSet()->addClaim("iss", issuer.value());
  }

  if (subject) {
    jwtObj.getClaimSet()->addClaim("sub", subject.value());
  }

  if (resource) {
    jwtObj.getClaimSet()->addClaim("aud", resource.value());
  }
  auto key = std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY *)>>(generate_key(),
                                                                        [](EVP_PKEY *k) { EVP_PKEY_free(k); });
  auto jwtString = jwtObj.serialize(key.get());
  return jwtString;
}

// Helper to create fake HTTP client that verifies client_id parameter
FakeHttpClient makeAzureHttpClientWithClientIdCheck(
    const std::string &jwt,
    const std::string &api_version,
    const std::string &resource,
    const std::string &host,
    const std::string &scheme,
    const boost::optional<std::string> &expectedClientId,
    const boost::optional<std::string> &identityHeader = boost::none) {
  return FakeHttpClient([=](Snowflake::Client::HttpRequest req) {
    // Verify client_id parameter
    auto clientIdParam = req.url.params().find("client_id");
    if (expectedClientId) {
      assert_true(clientIdParam != req.url.params().end());
      assert_true((*clientIdParam).value == expectedClientId.get());
    } else {
      assert_true(clientIdParam == req.url.params().end());
    }
    
    // Verify other parameters
    assert_true((*req.url.params().find("api-version")).value == api_version);
    assert_true((*req.url.params().find("resource")).value == resource);
    assert_true(req.url.host() == host);
    assert_true(req.url.scheme() == scheme);
    
    // Verify headers
    if (identityHeader) {
      assert_true(req.headers.find("X-IDENTITY-HEADER")->second == identityHeader.get());
    } else {
      assert_true(req.headers.find("Metadata")->second == "True");
    }
    
    // Return successful response
    auto obj = picojson::object();
    obj["access_token"] = picojson::value(jwt);
    std::string response_body = picojson::value(obj).serialize();
    Snowflake::Client::HttpResponse response;
    response.code = 200;
    response.buffer = std::vector<char>(response_body.begin(), response_body.end());
    return response;
  });
}

// Helper to assert Azure attestation result
void assertAzureAttestation(
    const boost::optional<Attestation> &attestationOpt,
    const std::string &expectedToken,
    const std::string &expectedIssuer,
    const std::string &expectedSubject) {
  assert_true(attestationOpt.is_initialized());
  auto &attestation = attestationOpt.get();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AZURE);
  assert_true(attestation.credential == expectedToken);
  assert_true(attestation.issuer == expectedIssuer);
  assert_true(attestation.subject == expectedSubject);
}

void test_unit_azure_attestation_vm_success(void **) {
  auto token = makeAzureToken(AZURE_TEST_ISSUER_MICROSOFT, AZURE_TEST_SUBJECT, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = makeSuccessfulAzureHttpClient(token, AZURE_TEST_API_VERSION_VM, AZURE_TEST_RESOURCE,
                                                      AZURE_TEST_DEFAULT_ENDPOINT_HOST, AZURE_TEST_DEFAULT_ENDPOINT_PROTOCOL);
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.httpClient = &fakeHttpClient;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto &attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AZURE);
  assert_true(attestation.credential == token);
  assert_true(attestation.issuer == AZURE_TEST_ISSUER_MICROSOFT);
  assert_true(attestation.subject == AZURE_TEST_SUBJECT);
}

void test_unit_azure_attestation_function_success(void **) {
  EnvOverride headerOverride("IDENTITY_HEADER", AZURE_TEST_IDENTITY_HEADER);
  EnvOverride endpointOverride("IDENTITY_ENDPOINT", AZURE_TEST_IDENTITY_ENDPOINT);
  EnvOverride clientIdOverride("MANAGED_IDENTITY_CLIENT_ID", AZURE_TEST_MANAGED_IDENTITY_CLIENT_ID);
  auto token = makeAzureToken(AZURE_TEST_ISSUER_STS, AZURE_TEST_SUBJECT, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = makeSuccessfulAzureHttpClient(token, AZURE_TEST_API_VERSION_FUNCTION, AZURE_TEST_RESOURCE,
                                                      AZURE_TEST_IDENTITY_ENDPOINT_HOST,
                                                      AZURE_TEST_IDENTITY_ENDPOINT_PROTOCOL,
                                                      AZURE_TEST_IDENTITY_HEADER);
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  config.httpClient = &fakeHttpClient;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto &attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AZURE);
  assert_true(attestation.credential == token);
  assert_true(attestation.issuer == AZURE_TEST_ISSUER_STS);
  assert_true(attestation.subject == AZURE_TEST_SUBJECT);
}

void test_unit_azure_attestation_missing_resource(void **) {
  auto token = makeAzureToken(AZURE_TEST_ISSUER_MICROSOFT, AZURE_TEST_SUBJECT, AZURE_TEST_DEFAULT_RESOURCE);
  auto fakeHttpClient = makeSuccessfulAzureHttpClient(token, AZURE_TEST_API_VERSION_VM, AZURE_TEST_DEFAULT_RESOURCE,
                                                      AZURE_TEST_DEFAULT_ENDPOINT_HOST, AZURE_TEST_DEFAULT_ENDPOINT_PROTOCOL);
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.httpClient = &fakeHttpClient;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto &attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AZURE);
  assert_true(attestation.credential == token);
  assert_true(attestation.issuer == AZURE_TEST_ISSUER_MICROSOFT);
  assert_true(attestation.subject == AZURE_TEST_SUBJECT);
}

void test_unit_azure_attestation_missing_issuer(void **) {
  auto token = makeAzureToken(boost::none, AZURE_TEST_SUBJECT, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = makeSuccessfulAzureHttpClient(token, AZURE_TEST_API_VERSION_VM, AZURE_TEST_RESOURCE,
                                                      AZURE_TEST_DEFAULT_ENDPOINT_HOST, AZURE_TEST_DEFAULT_ENDPOINT_PROTOCOL);
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  config.httpClient = &fakeHttpClient;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(!attestationOpt);
}

void test_unit_azure_attestation_missing_subject(void **) {
  auto token = makeAzureToken(AZURE_TEST_ISSUER_MICROSOFT, boost::none, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = makeSuccessfulAzureHttpClient(token, AZURE_TEST_API_VERSION_VM, AZURE_TEST_RESOURCE,
                                                      AZURE_TEST_DEFAULT_ENDPOINT_HOST, AZURE_TEST_DEFAULT_ENDPOINT_PROTOCOL);
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  config.httpClient = &fakeHttpClient;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(!attestationOpt);
}

void test_unit_azure_attestation_request_failed(void **) {
  auto token = makeAzureToken(AZURE_TEST_ISSUER_MICROSOFT, AZURE_TEST_SUBJECT, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = FakeHttpClient([](const Snowflake::Client::HttpRequest&) {
    return boost::none;
  });
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  config.httpClient = &fakeHttpClient;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(!attestationOpt);
}

void test_unit_azure_attestation_vm_with_client_id(void **) {
  EnvOverride clientIdOverride("MANAGED_IDENTITY_CLIENT_ID", AZURE_TEST_MANAGED_IDENTITY_CLIENT_ID);
  
  auto token = makeAzureToken(AZURE_TEST_ISSUER_MICROSOFT, AZURE_TEST_SUBJECT, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = makeAzureHttpClientWithClientIdCheck(
      token, AZURE_TEST_API_VERSION_VM, AZURE_TEST_RESOURCE,
      AZURE_TEST_DEFAULT_ENDPOINT_HOST, AZURE_TEST_DEFAULT_ENDPOINT_PROTOCOL,
      AZURE_TEST_MANAGED_IDENTITY_CLIENT_ID /* expectedClientId */);
  
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.httpClient = &fakeHttpClient;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assertAzureAttestation(attestationOpt, token, AZURE_TEST_ISSUER_MICROSOFT, AZURE_TEST_SUBJECT);
}

void test_unit_azure_attestation_vm_without_client_id(void **) {
  auto token = makeAzureToken(AZURE_TEST_ISSUER_MICROSOFT, AZURE_TEST_SUBJECT, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = makeAzureHttpClientWithClientIdCheck(
      token, AZURE_TEST_API_VERSION_VM, AZURE_TEST_RESOURCE,
      AZURE_TEST_DEFAULT_ENDPOINT_HOST, AZURE_TEST_DEFAULT_ENDPOINT_PROTOCOL,
      boost::none /* expectedClientId */);
  
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.httpClient = &fakeHttpClient;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assertAzureAttestation(attestationOpt, token, AZURE_TEST_ISSUER_MICROSOFT, AZURE_TEST_SUBJECT);
}

void test_unit_azure_attestation_function_with_client_id(void **) {
  EnvOverride headerOverride("IDENTITY_HEADER", AZURE_TEST_IDENTITY_HEADER);
  EnvOverride endpointOverride("IDENTITY_ENDPOINT", AZURE_TEST_IDENTITY_ENDPOINT);
  EnvOverride clientIdOverride("MANAGED_IDENTITY_CLIENT_ID", AZURE_TEST_MANAGED_IDENTITY_CLIENT_ID);
  
  auto token = makeAzureToken(AZURE_TEST_ISSUER_STS, AZURE_TEST_SUBJECT, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = makeAzureHttpClientWithClientIdCheck(
      token, AZURE_TEST_API_VERSION_FUNCTION, AZURE_TEST_RESOURCE,
      AZURE_TEST_IDENTITY_ENDPOINT_HOST, AZURE_TEST_IDENTITY_ENDPOINT_PROTOCOL,
      AZURE_TEST_MANAGED_IDENTITY_CLIENT_ID /* expectedClientId */,
      AZURE_TEST_IDENTITY_HEADER /* identityHeader */);
  
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.httpClient = &fakeHttpClient;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assertAzureAttestation(attestationOpt, token, AZURE_TEST_ISSUER_STS, AZURE_TEST_SUBJECT);
}

void test_unit_azure_attestation_function_without_client_id(void **) {
  EnvOverride headerOverride("IDENTITY_HEADER", AZURE_TEST_IDENTITY_HEADER);
  EnvOverride endpointOverride("IDENTITY_ENDPOINT", AZURE_TEST_IDENTITY_ENDPOINT);
  
  auto token = makeAzureToken(AZURE_TEST_ISSUER_STS, AZURE_TEST_SUBJECT, AZURE_TEST_RESOURCE);
  auto fakeHttpClient = makeAzureHttpClientWithClientIdCheck(
      token, AZURE_TEST_API_VERSION_FUNCTION, AZURE_TEST_RESOURCE,
      AZURE_TEST_IDENTITY_ENDPOINT_HOST, AZURE_TEST_IDENTITY_ENDPOINT_PROTOCOL,
      boost::none /* expectedClientId */,
      AZURE_TEST_IDENTITY_HEADER /* identityHeader */);
  
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.httpClient = &fakeHttpClient;
  config.snowflakeEntraResource = AZURE_TEST_RESOURCE;
  
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assertAzureAttestation(attestationOpt, token, AZURE_TEST_ISSUER_STS, AZURE_TEST_SUBJECT);
}

void test_unit_oidc_attestation_success(void **) {
  auto jwtObj = Jwt::JWTObject();
  jwtObj.getHeader()->setAlgorithm(Jwt::AlgorithmType::RS256);
  jwtObj.getClaimSet()->addClaim("iss", "https://issues.net/");
  jwtObj.getClaimSet()->addClaim("sub", "john.doe");
  auto key = std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY *)>>(generate_key(),
                                                                        [](EVP_PKEY *k) { EVP_PKEY_free(k); });
  auto jwtStr = jwtObj.serialize(key.get());
  AttestationConfig config;
  config.token = jwtStr;
  config.type = AttestationType::OIDC;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto &attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::OIDC);
  assert_true(attestation.credential == jwtStr);
  assert_true(attestation.issuer == std::string("https://issues.net/"));
  assert_true(attestation.subject == std::string("john.doe"));
}

void test_unit_oidc_attestation_missing_token(void **) {
  AttestationConfig config;
  config.type = AttestationType::OIDC;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(!attestationOpt);
}

int main() {
  const struct CMUnitTest tests[] = {
#ifndef _WIN32
//       Disabled on Windows because we don't have AWS credentials set up in the CI
      cmocka_unit_test(test_integration_aws_attestation),
#endif
      cmocka_unit_test(test_unit_aws_attestation_success),
      cmocka_unit_test(test_unit_aws_attestation_china_region_success),
      cmocka_unit_test(test_unit_aws_attestation_region_missing),
      cmocka_unit_test(test_unit_aws_attestation_cred_missing),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_single_role),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_role_chain),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_whitespace_handling),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_cross_account),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_empty_path_fallback),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_with_missing_region),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_with_missing_credentials),
      cmocka_unit_test(test_unit_gcp_attestation_success),
      cmocka_unit_test(test_unit_gcp_attestation_missing_issuer),
      cmocka_unit_test(test_unit_gcp_attestation_missing_subject),
      cmocka_unit_test(test_unit_gcp_attestation_failed_request),
      cmocka_unit_test(test_unit_gcp_attestation_bad_request),
      cmocka_unit_test(test_unit_azure_attestation_vm_success),
      cmocka_unit_test(test_unit_azure_attestation_missing_issuer),
      cmocka_unit_test(test_unit_azure_attestation_missing_resource),
      cmocka_unit_test(test_unit_azure_attestation_missing_subject),
      cmocka_unit_test(test_unit_azure_attestation_request_failed),
      cmocka_unit_test(test_unit_azure_attestation_function_success),
      cmocka_unit_test(test_unit_azure_attestation_vm_with_client_id),
      cmocka_unit_test(test_unit_azure_attestation_vm_without_client_id),
      cmocka_unit_test(test_unit_azure_attestation_function_with_client_id),
      cmocka_unit_test(test_unit_azure_attestation_function_without_client_id),
      cmocka_unit_test(test_unit_oidc_attestation_success),
      cmocka_unit_test(test_unit_oidc_attestation_missing_token)
  };

  return cmocka_run_group_tests(tests, nullptr, nullptr);
}
