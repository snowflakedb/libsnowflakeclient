
#include <memory>
#include <utility>

#include <boost/url.hpp>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include "snowflake/TomlConfigParser.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "utils/EnvOverride.hpp"
#include "snowflake/WifAttestation.hpp"
#include "util/Base64.hpp"
#include <curl/curl.h>
#include <jwt/Jwt.hpp>

using namespace Snowflake::Client;

namespace {
  EVP_PKEY *generate_key()
  {
    EVP_PKEY *key = nullptr;

    std::unique_ptr<EVP_PKEY_CTX, std::function<void(EVP_PKEY_CTX *)>>
        kct{EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), [](EVP_PKEY_CTX *ctx) { EVP_PKEY_CTX_free(ctx); }};

    if (EVP_PKEY_keygen_init(kct.get()) <= 0) return nullptr;

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(kct.get(), 2048) <= 0) return nullptr;

    if (EVP_PKEY_keygen(kct.get(), &key) <= 0) return nullptr;

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


long run_request_curl(
    const std::string& url,
    const std::string& method,
    const std::map<std::string, std::string>& headers)
{
  CURL* curl = curl_easy_init();
  assert_true(curl != nullptr);

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

  struct curl_slist* header_list = nullptr;
  for (const auto& h : headers) {
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

void test_aws_attestation(void** state)
{
  AttestationConfig config;
  config.type = AttestationType::AWS;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto& attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AWS);
  assert_true(attestation.credential.size() > 0);
  std::string json_string;
  Snowflake::Client::Util::Base64::decodePadding(attestation.credential.begin(), attestation.credential.end(), std::back_inserter(json_string));
  picojson::value json;
  picojson::parse(json, json_string);
  assert_true(json.is<picojson::object>());
  assert_true(json.get("url").is<std::string>());
  assert_true(json.get("method").is<std::string>());
  std::string method = json.get("method").get<std::string>();
  assert_true(json.get("headers").is<picojson::object>());
  std::map<std::string, std::string> headers;
  for (auto& header: json.get("headers").get<picojson::object>()) {
    assert_true(header.second.is<std::string>());
    headers[header.first] = header.second.get<std::string>();
  }
  assert_true(
    run_request_curl(json.get("url").get<std::string>(), method, headers) ==
    200
  );
}

void test_gcp_attestation(void** state)
{
  auto audience = "snowflakecomputing.com";
  auto jwtObj = Jwt::JWTObject();
  jwtObj.getHeader()->setAlgorithm(Jwt::AlgorithmType::RS256);
  jwtObj.getClaimSet()->addClaim("iss", "https://accounts.google.com");
  jwtObj.getClaimSet()->addClaim("sub", "107562638633288735786");
  auto key = std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY*)>>(generate_key(), [](EVP_PKEY* k) { EVP_PKEY_free(k); });
  auto jwtString = jwtObj.serialize(key.get());
  auto fakeHttpClient = FakeHttpClient([&](Snowflake::Client::HttpRequest req) {
    assert_true((*req.url.params().find("audience")).value == audience);
    assert_true(req.url.host() == "169.254.169.254");
    assert_true(req.url.scheme() == "http");
    HttpResponse response;
    response.code = 200;
    response.m_buffer = std::vector<char>(jwtString.begin(), jwtString.end());
    return response;
  });
  AttestationConfig config;
  config.type = AttestationType::GCP;
  config.httpClient = &fakeHttpClient;
  auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  const auto& attestation = attestationOpt.get();
  assert_true(attestation.type == Snowflake::Client::AttestationType::GCP);
  assert_true(attestation.credential == jwtString);
  assert_true(attestation.subject == "107562638633288735786");
}

void test_azure_attestation(void** state)
{
  auto resource = "api://4f6b1daa-cbcb-4342-ac74-005fbc825d2a";
  auto jwtObj = Jwt::JWTObject();
  jwtObj.getHeader()->setAlgorithm(Jwt::AlgorithmType::RS256);
  jwtObj.getClaimSet()->addClaim("iss", "https://sts.windows.net/f05bdcc4-50e7-4fea-958d-32cdb12b3aca/");
  jwtObj.getClaimSet()->addClaim("sub", "f05bdcc4-50e7-4fea-958d-32cdb12b3aca");
  jwtObj.getClaimSet()->addClaim("aud", resource);
  auto key = std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY*)>>(generate_key(), [](EVP_PKEY* k) { EVP_PKEY_free(k); });
  auto jwtString = jwtObj.serialize(key.get());
  auto fakeHttpClient = FakeHttpClient([&](Snowflake::Client::HttpRequest req) {
    assert_true((*req.url.params().find("api-version")).value == "2018-02-01");
    assert_true((*req.url.params().find("resource")).value == resource);
    assert_true(req.url.host() == "169.254.169.254");
    assert_true(req.url.scheme() == "http");
    auto obj = picojson::object();
    obj["access_token"] = picojson::value(jwtString);
    std::string response_body = picojson::value(obj).serialize();
    Snowflake::Client::HttpResponse response;
    response.code = 200;
    response.m_buffer = std::vector<char>(response_body.begin(), response_body.end());
    return response;
  });
  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.snowflakeEntraResource = resource;
  config.httpClient = &fakeHttpClient;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto& attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AZURE);
  assert_true(attestation.credential == jwtString);
  assert_true(attestation.issuer == "https://sts.windows.net/f05bdcc4-50e7-4fea-958d-32cdb12b3aca/");
  assert_true(attestation.subject == "f05bdcc4-50e7-4fea-958d-32cdb12b3aca");
}

void test_oidc_attestation(void** state)
{
  auto jwtObj = Jwt::JWTObject();
  jwtObj.getHeader()->setAlgorithm(Jwt::AlgorithmType::RS256);
  jwtObj.getClaimSet()->addClaim("iss", "https://issues.net/");
  jwtObj.getClaimSet()->addClaim("sub", "john.doe");
  auto key = std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY*)>>(generate_key(), [](EVP_PKEY* k) { EVP_PKEY_free(k); });
  auto jwtStr = jwtObj.serialize(key.get());
  AttestationConfig config;
  config.token = jwtStr;
  config.type = AttestationType::OIDC;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto& attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::OIDC);
  assert_true(attestation.credential == jwtStr);
  assert_true(attestation.issuer == "https://issues.net/");
  assert_true(attestation.subject == "john.doe");
}

int main()
{
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_aws_attestation),
      cmocka_unit_test(test_gcp_attestation),
      cmocka_unit_test(test_azure_attestation),
      cmocka_unit_test(test_oidc_attestation)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
