#include <map>
#include <memory>
#include <utility>
#include <vector>
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

  // Stubs STS:AssumeRole. Looks up the role ARN in `assumeRoleResults`; if
  // present returns the canned credentials, otherwise returns boost::none
  // (matching the production behavior when AssumeRole fails). Records
  // every call so tests can assert on chain ordering and per-step inputs.
  boost::optional<Aws::Auth::AWSCredentials> assumeRole(
      const Aws::Auth::AWSCredentials& currentCreds,
      const std::string& roleArn) override {
    assumeRoleCallCount++;
    assumeRoleArns.push_back(roleArn);
    assumeRoleInputCreds.push_back(currentCreds);
    auto it = assumeRoleResults.find(roleArn);
    if (it == assumeRoleResults.end()) {
      return boost::none;
    }
    return it->second;
  }

  boost::optional<std::string> getWebIdentityToken(
      const Aws::Auth::AWSCredentials& c,
      const std::string& r,
      const std::string& aud,
      const std::string& alg,
      const std::string& host) override {
    getWebIdentityTokenCallCount++;
    lastGetWebIdentityTokenCreds = c;
    lastGetWebIdentityTokenRegion = r;
    lastGetWebIdentityTokenAudience = aud;
    lastGetWebIdentityTokenAlgorithm = alg;
    lastGetWebIdentityTokenHost = host;
    return webIdentityTokenResult;
  }

  // Test knobs.
  boost::optional<std::string> webIdentityTokenResult;
  std::map<std::string, boost::optional<Aws::Auth::AWSCredentials>> assumeRoleResults;

  // Call recorders for assertions.
  int getWebIdentityTokenCallCount = 0;
  Aws::Auth::AWSCredentials lastGetWebIdentityTokenCreds;
  std::string lastGetWebIdentityTokenRegion;
  std::string lastGetWebIdentityTokenAudience;
  std::string lastGetWebIdentityTokenAlgorithm;
  std::string lastGetWebIdentityTokenHost;

  int assumeRoleCallCount = 0;
  std::vector<std::string> assumeRoleArns;
  std::vector<Aws::Auth::AWSCredentials> assumeRoleInputCreds;

private:
  boost::optional<std::string> region;
  Aws::Auth::AWSCredentials creds;
};


const std::string AWS_TEST_REGION = "us-east-1";
const Aws::Auth::AWSCredentials AWS_TEST_CREDS = Aws::Auth::AWSCredentials("AKIAEXAMPLE12345678",
                                                                           "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"); // pragma: allowlist secret

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

const std::string FAKE_WEB_IDENTITY_TOKEN = "fake.jwt.token-for-testing-only";

void test_unit_aws_attestation_jwt_success(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  awsSdkWrapper.webIdentityTokenResult = FAKE_WEB_IDENTITY_TOKEN;

  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;

  const auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  const auto &attestation = attestationOpt.get();

  assert_true(attestation.type == AttestationType::AWS);
  // The JWT is the credential — not wrapped in base64(JSON).
  assert_true(attestation.credential == FAKE_WEB_IDENTITY_TOKEN);
  // GS resolves issuer/subject from the JWT claims; driver leaves them unset.
  assert_true(!attestation.issuer);
  assert_true(!attestation.subject);

  // STS call shape matches the Python implementation.
  assert_int_equal(awsSdkWrapper.getWebIdentityTokenCallCount, 1);
  assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenAudience.c_str(),
                     "snowflakecomputing.com");
  assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenAlgorithm.c_str(),
                     "ES384");
  assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenRegion.c_str(),
                     AWS_TEST_REGION.c_str());
  // No impersonation -> creds passed through unchanged.
  assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenCreds.GetAWSAccessKeyId().c_str(),
                     AWS_TEST_CREDS.GetAWSAccessKeyId().c_str());
}

void test_unit_aws_attestation_jwt_success_with_custom_wif_config(void**) {
    auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
    awsSdkWrapper.webIdentityTokenResult = FAKE_WEB_IDENTITY_TOKEN;

    AttestationConfig config;
    config.type = AttestationType::AWS;
    config.awsSdkWrapper = &awsSdkWrapper;
    config.audience = "custom-audience-for-testing";
    config.wifHost = "custom-wif-host-for-testing";

    const auto attestationOpt = createAttestation(config);
    assert_true(attestationOpt.has_value());
    const auto& attestation = attestationOpt.get();

    assert_true(attestation.type == AttestationType::AWS);
    // The JWT is the credential — not wrapped in base64(JSON).
    assert_true(attestation.credential == FAKE_WEB_IDENTITY_TOKEN);
    // GS resolves issuer/subject from the JWT claims; driver leaves them unset.
    assert_true(!attestation.issuer);
    assert_true(!attestation.subject);

    // STS call shape matches the Python implementation.
    assert_int_equal(awsSdkWrapper.getWebIdentityTokenCallCount, 1);
    assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenAudience.c_str(),
        "custom-audience-for-testing");
    assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenHost.c_str(),
        "custom-wif-host-for-testing");
    assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenAlgorithm.c_str(),
        "ES384");
    assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenRegion.c_str(),
        AWS_TEST_REGION.c_str());
    // No impersonation -> creds passed through unchanged.
    assert_string_equal(awsSdkWrapper.lastGetWebIdentityTokenCreds.GetAWSAccessKeyId().c_str(),
        AWS_TEST_CREDS.GetAWSAccessKeyId().c_str());
}

void test_unit_aws_attestation_jwt_sdk_failure(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  // STS call returns no token (e.g. HTTP error, malformed response).
  awsSdkWrapper.webIdentityTokenResult = boost::none;

  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;

  const auto attestationOpt = createAttestation(config);
  // Must fail closed: no silent fallback to a different credential format.
  assert_false(attestationOpt.has_value());
  assert_int_equal(awsSdkWrapper.getWebIdentityTokenCallCount, 1);
}

// With impersonation set, the assumed credentials MUST be the ones handed to
// getWebIdentityToken. Otherwise we'd bypass impersonation and use the
// initial creds for the JWT — a security bug.
void test_unit_aws_attestation_jwt_with_impersonation(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  const std::string roleArn = "arn:aws:iam::123456789012:role/TestRole";
  const Aws::Auth::AWSCredentials assumedCreds(
      "ASSUMED_ACCESS_KEY", "ASSUMED_SECRET_KEY", "ASSUMED_SESSION_TOKEN");
  awsSdkWrapper.assumeRoleResults[roleArn] = assumedCreds;
  awsSdkWrapper.webIdentityTokenResult = FAKE_WEB_IDENTITY_TOKEN;

  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = roleArn;

  const auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  assert_true(attestationOpt.get().credential == FAKE_WEB_IDENTITY_TOKEN);

  // Impersonation actually ran with the configured ARN, using the initial
  // creds as input.
  assert_int_equal(awsSdkWrapper.assumeRoleCallCount, 1);
  assert_string_equal(awsSdkWrapper.assumeRoleArns[0].c_str(), roleArn.c_str());
  assert_string_equal(
      awsSdkWrapper.assumeRoleInputCreds[0].GetAWSAccessKeyId().c_str(),
      AWS_TEST_CREDS.GetAWSAccessKeyId().c_str());

  // getWebIdentityToken received the ASSUMED creds, NOT the initial ones.
  assert_int_equal(awsSdkWrapper.getWebIdentityTokenCallCount, 1);
  assert_string_equal(
      awsSdkWrapper.lastGetWebIdentityTokenCreds.GetAWSAccessKeyId().c_str(),
      "ASSUMED_ACCESS_KEY");
  assert_string_equal(
      awsSdkWrapper.lastGetWebIdentityTokenCreds.GetAWSSecretKey().c_str(),
      "ASSUMED_SECRET_KEY");
  assert_string_equal(
      awsSdkWrapper.lastGetWebIdentityTokenCreds.GetSessionToken().c_str(),
      "ASSUMED_SESSION_TOKEN");
}

// Chain variant: each step's output feeds the next step's input, and the
// final step's output is what reaches getWebIdentityToken.
void test_unit_aws_attestation_jwt_with_impersonation_chain(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  const std::string arn1 = "arn:aws:iam::111111111111:role/Role1";
  const std::string arn2 = "arn:aws:iam::222222222222:role/Role2";
  const Aws::Auth::AWSCredentials creds1("AK1", "SK1", "ST1");
  const Aws::Auth::AWSCredentials creds2("AK2", "SK2", "ST2");
  awsSdkWrapper.assumeRoleResults[arn1] = creds1;
  awsSdkWrapper.assumeRoleResults[arn2] = creds2;
  awsSdkWrapper.webIdentityTokenResult = FAKE_WEB_IDENTITY_TOKEN;

  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = arn1 + "," + arn2;

  const auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  assert_true(attestationOpt.get().credential == FAKE_WEB_IDENTITY_TOKEN);

  // Chain visited in order; step 2 ran with step 1's output.
  assert_int_equal(awsSdkWrapper.assumeRoleCallCount, 2);
  assert_string_equal(awsSdkWrapper.assumeRoleArns[0].c_str(), arn1.c_str());
  assert_string_equal(awsSdkWrapper.assumeRoleArns[1].c_str(), arn2.c_str());
  assert_string_equal(
      awsSdkWrapper.assumeRoleInputCreds[0].GetAWSAccessKeyId().c_str(),
      AWS_TEST_CREDS.GetAWSAccessKeyId().c_str());
  assert_string_equal(
      awsSdkWrapper.assumeRoleInputCreds[1].GetAWSAccessKeyId().c_str(),
      "AK1");

  // Final assumed creds reach getWebIdentityToken.
  assert_string_equal(
      awsSdkWrapper.lastGetWebIdentityTokenCreds.GetAWSAccessKeyId().c_str(),
      "AK2");
}

// If the impersonation chain fails (e.g. STS error on one step), no JWT is
// fetched and the attestation fails — we never silently fall back to the
// initial creds.
void test_unit_aws_attestation_jwt_impersonation_failure(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  // No entry in assumeRoleResults -> assumeRole returns boost::none.
  awsSdkWrapper.webIdentityTokenResult = FAKE_WEB_IDENTITY_TOKEN;

  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = "arn:aws:iam::123456789012:role/TestRole";

  const auto attestationOpt = createAttestation(config);
  assert_false(attestationOpt.has_value());
  assert_int_equal(awsSdkWrapper.assumeRoleCallCount, 1);
  assert_int_equal(awsSdkWrapper.getWebIdentityTokenCallCount, 0);
}

// Whitespace around comma-separated ARNs is trimmed before being passed to
// assumeRole. The other JWT-impersonation tests already cover ordering,
// chaining, cross-account ARNs, and end-to-end success / failure paths; this
// one only verifies the trimming property.
void test_unit_aws_attestation_impersonation_whitespace_trimming(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  const std::string arn1 = "arn:aws:iam::123456789012:role/Role1";
  const std::string arn2 = "arn:aws:iam::123456789012:role/Role2";
  awsSdkWrapper.assumeRoleResults[arn1] = Aws::Auth::AWSCredentials("AK1", "SK1", "ST1");
  awsSdkWrapper.assumeRoleResults[arn2] = Aws::Auth::AWSCredentials("AK2", "SK2", "ST2");
  awsSdkWrapper.webIdentityTokenResult = FAKE_WEB_IDENTITY_TOKEN;

  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = "  " + arn1 + "  , " + arn2 + " ";

  const auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());

  assert_int_equal(awsSdkWrapper.assumeRoleCallCount, 2);
  assert_string_equal(awsSdkWrapper.assumeRoleArns[0].c_str(), arn1.c_str());
  assert_string_equal(awsSdkWrapper.assumeRoleArns[1].c_str(), arn2.c_str());
}

// An empty impersonation path is treated as "no impersonation": no
// assumeRole calls, and getWebIdentityToken receives the initial creds
// unchanged.
void test_unit_aws_attestation_impersonation_empty_path_fallback(void **) {
  auto awsSdkWrapper = FakeAwsSdkWrapper(AWS_TEST_REGION, AWS_TEST_CREDS);
  awsSdkWrapper.webIdentityTokenResult = FAKE_WEB_IDENTITY_TOKEN;

  AttestationConfig config;
  config.type = AttestationType::AWS;
  config.awsSdkWrapper = &awsSdkWrapper;
  config.workloadIdentityImpersonationPath = std::string("");

  auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  assert_true(attestationOpt.get().type == AttestationType::AWS);

  assert_int_equal(awsSdkWrapper.assumeRoleCallCount, 0);
  assert_int_equal(awsSdkWrapper.getWebIdentityTokenCallCount, 1);
  assert_string_equal(
      awsSdkWrapper.lastGetWebIdentityTokenCreds.GetAWSAccessKeyId().c_str(),
      AWS_TEST_CREDS.GetAWSAccessKeyId().c_str());
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
const std::string GCP_TEST_CUSTOM_ENDPOINT_HOST = "iamcredentials.privategoogleapis.com";

// Multi-path fake HTTP client for GCP service account impersonation
enum class AcceptedHosts {
  Metadata,
  Iam,
  Other
};

auto getHost(const std::string& host) -> AcceptedHosts {
  if (host == GCP_TEST_METADATA_ENDPOINT_HOST) return AcceptedHosts::Metadata;
  if (host == GCP_TEST_IAM_ENDPOINT_HOST || host == GCP_TEST_CUSTOM_ENDPOINT_HOST) return AcceptedHosts::Iam;
  return AcceptedHosts::Other;
}

FakeHttpClient makeSuccessfulGCPImpersonationHttpClient(
    const std::vector<char>& accessToken,
    const std::vector<char>& idToken,
    const std::vector<std::string>& expectedDelegates,
    const std::string& expectedTargetServiceAccount,
    const std::string& expectedAudience = GCP_TEST_AUDIENCE,
    const std::string& expectedWifHost = "") {
  return FakeHttpClient([=](Snowflake::Client::HttpRequest req) {
    HttpResponse response;
    response.code = 200;

    switch (getHost(req.url.host())) {
      case AcceptedHosts::Metadata: {
        if (req.url.encoded_path() == "/computeMetadata/v1/instance/service-accounts/default/token") {
          assert_true(req.headers.find("Metadata-Flavor")->second == "Google");
          picojson::object tokenResponse;
          tokenResponse["access_token"] = picojson::value(std::string(accessToken.data(), accessToken.size()));
          tokenResponse["expires_in"] = picojson::value(static_cast<double>(3600));
          tokenResponse["token_type"] = picojson::value("Bearer");
          std::string jsonStr = picojson::value(tokenResponse).serialize();
          response.buffer = std::vector<char>(jsonStr.begin(), jsonStr.end());
        }
        break;
      }
      case AcceptedHosts::Iam: {
        std::string expectedPath = "/v1/projects/-/serviceAccounts/" +
                                   expectedTargetServiceAccount + ":generateIdToken";
        assert_true(req.url.encoded_path() == expectedPath);
        assert_string_equal(req.url.host().c_str(), expectedWifHost.empty() ? GCP_TEST_IAM_ENDPOINT_HOST.c_str() : GCP_TEST_CUSTOM_ENDPOINT_HOST.c_str());
        assert_true(req.method == HttpRequest::Method::POST);
        const auto accessTokenStr = std::string(accessToken.data(), accessToken.size());
        assert_true(req.headers.find("Authorization")->second == "Bearer " + accessTokenStr);
        assert_true(req.headers.find("Content-Type")->second == "application/json");

        picojson::value bodyJson;
        std::string err = picojson::parse(bodyJson, req.body);
        assert_true(err.empty());
        assert_true(bodyJson.is<picojson::object>());

        auto bodyObj = bodyJson.get<picojson::object>();
        assert_true(bodyObj["audience"].get<std::string>() == expectedAudience);
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

        picojson::object idTokenResponse;
        idTokenResponse["token"] = picojson::value(std::string(idToken.data(), idToken.size()));
        std::string jsonStr = picojson::value(idTokenResponse).serialize();
        response.buffer = std::vector<char>(jsonStr.begin(), jsonStr.end());
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

void test_unit_gcp_impersonation_single_account_success_with_custom_wif_cofig(void**) {
    const auto accessToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT_ACCESS);
    const auto idToken = makeGCPToken(GCP_TEST_ISSUER, GCP_TEST_SUBJECT);
    const std::string targetServiceAccount = "target@project.iam.gserviceaccount.com";
    const std::string testingAudience = "custom-audience-for-testing";
    const std::string testingHost = "https://" + GCP_TEST_CUSTOM_ENDPOINT_HOST + "/v1";

    auto fakeHttpClient = makeSuccessfulGCPImpersonationHttpClient(
        accessToken,
        idToken,
        {},
        targetServiceAccount,
        testingAudience,
        testingHost);

    AttestationConfig config;
    config.type = AttestationType::GCP;
    config.httpClient = &fakeHttpClient;
    config.workloadIdentityImpersonationPath = targetServiceAccount;
    config.audience = testingAudience;
    config.wifHost = testingHost;

    const auto attestationOpt = createAttestation(config);
    assert_true(attestationOpt.has_value());
    const auto& [type, credential, issuer, subject] = attestationOpt.get();
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

void test_unit_wif_attestation_config(void**)
{
    AttestationConfig config;
    SF_CONNECT* conn = snowflake_init();
    snowflake_set_attribute(conn, SF_CON_WIF_AZURE_RESOURCE, "dummy_resource");
    
    assert_int_equal(config.configureWIFAttestation(conn), SF_STATUS_ERROR_GENERAL);
    assert_false(config.snowflakeEntraResource.has_value());

    snowflake_set_attribute(conn, SF_CON_WIF_PROVIDER, "AWS");
    assert_int_equal(config.configureWIFAttestation(conn), SF_STATUS_SUCCESS);

    assert_true(config.type.has_value());
    assert_int_equal(config.type.get(), AttestationType::AWS);

    assert_false(config.audience.has_value());
    assert_string_equal(config.getAudience().c_str(), SF_SNOWFLAKE_WIF_AUDIENCE);

    assert_true(config.snowflakeEntraResource.has_value());
    assert_string_equal(config.snowflakeEntraResource.get().c_str(), "dummy_resource");
    assert_string_equal(config.getWifHost().c_str(), "");

    snowflake_set_attribute(conn, SF_CON_WIF_PROVIDER, "GCP");
    snowflake_set_attribute(conn, SF_CON_WIF_AUDIENCE, "dummy_audience.com");
    snowflake_set_attribute(conn, SF_CON_WORKLOAD_IDENTITY_IMPERSONATION_PATH, "dummy_impersonation_path");
    snowflake_set_attribute(conn, SF_CON_WIF_TOKEN, "dummy_token");
    snowflake_set_attribute(conn, SF_CON_WIF_HOST, "dummy_host");


    assert_int_equal(config.configureWIFAttestation(conn), SF_STATUS_SUCCESS);

    assert_true(config.type.has_value());
    assert_int_equal(config.type.get(), AttestationType::GCP);

    assert_true(config.workloadIdentityImpersonationPath.has_value());
    assert_string_equal(config.workloadIdentityImpersonationPath.get().c_str(), "dummy_impersonation_path");

    assert_true(config.token.has_value());
    assert_string_equal(config.token.get().c_str(), "dummy_token");

    assert_true(config.audience.has_value());
    assert_string_equal(config.getAudience().c_str(), "dummy_audience.com");

    assert_true(config.snowflakeEntraResource.has_value());
    assert_string_equal(config.snowflakeEntraResource.get().c_str(), "dummy_resource");
    assert_string_equal(config.getWifHost().c_str(), "dummy_host");

    snowflake_term(conn);
}

int main() {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_unit_aws_attestation_region_missing),
      cmocka_unit_test(test_unit_aws_attestation_cred_missing),
      cmocka_unit_test(test_unit_aws_attestation_jwt_success),
      cmocka_unit_test(test_unit_aws_attestation_jwt_success_with_custom_wif_config),
      cmocka_unit_test(test_unit_aws_attestation_jwt_sdk_failure),
      cmocka_unit_test(test_unit_aws_attestation_jwt_with_impersonation),
      cmocka_unit_test(test_unit_aws_attestation_jwt_with_impersonation_chain),
      cmocka_unit_test(test_unit_aws_attestation_jwt_impersonation_failure),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_whitespace_trimming),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_empty_path_fallback),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_with_missing_region),
      cmocka_unit_test(test_unit_aws_attestation_impersonation_with_missing_credentials),
      cmocka_unit_test(test_unit_gcp_attestation_success),
      cmocka_unit_test(test_unit_gcp_attestation_missing_issuer),
      cmocka_unit_test(test_unit_gcp_attestation_missing_subject),
      cmocka_unit_test(test_unit_gcp_attestation_failed_request),
      cmocka_unit_test(test_unit_gcp_attestation_bad_request),
      cmocka_unit_test(test_unit_gcp_impersonation_single_account_success),
      cmocka_unit_test(test_unit_gcp_impersonation_single_account_success_with_custom_wif_cofig),
      cmocka_unit_test(test_unit_gcp_impersonation_chain_success),
      cmocka_unit_test(test_unit_gcp_impersonation_whitespace_in_path),
      cmocka_unit_test(test_unit_gcp_impersonation_access_token_failed),
      cmocka_unit_test(test_unit_gcp_impersonation_id_token_failed),
      cmocka_unit_test(test_unit_gcp_impersonation_empty_path),
      cmocka_unit_test(test_unit_gcp_impersonation_missing_token_in_response),
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
      cmocka_unit_test(test_unit_oidc_attestation_missing_token),
      cmocka_unit_test(test_unit_wif_attestation_config),
  };

  return cmocka_run_group_tests(tests, nullptr, nullptr);
}
