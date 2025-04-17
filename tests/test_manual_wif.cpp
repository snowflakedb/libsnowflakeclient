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
  auto type = std::getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE");
  if (!type || strcmp(type, "AWS") != 0) {
    std::cerr << "Not running AWS attestation test. SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE is not AWS" << std::endl;
    return;
  }
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
  HttpRequest req {
      HttpRequest::Method::GET,
      boost::url(json.get("url").get<std::string>()),
      headers
  };
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
  char* type = std::getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE");
  if (!type || strcmp(type, "GCP") != 0) {
    std::cerr << "Not running GCP attestation test. SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE is not GCP" << std::endl;
    return;
  }
  AttestationConfig config;
  config.type = AttestationType::GCP;
  std::cerr << "Creating GCP attestation" << std::endl;
  auto attestationOpt = createAttestation(config);
  assert_true(attestationOpt.has_value());
  const auto& attestation = attestationOpt.get();
  assert_true(attestation.type == Snowflake::Client::AttestationType::GCP);
  std::cerr << "Credential: " << attestation.credential << std::endl;
  assert_true(!attestation.credential.empty());
  std::cerr << "Subject: " << attestation.subject << std::endl;
  assert_true(!attestation.subject.empty());
}

void test_azure_attestation(void** state)
{
  char* type = std::getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE");
  if (!type || strcmp(type, "AZURE") != 0) {
    std::cerr << "Not running Azure attestation test. SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE is not AZURE" << std::endl;
    return;
  }

  char *resource = std::getenv("SNOWFLAKE_WIF_ATTESTATION_TEST_RESOURCE");
  if (!resource) {
    std::cerr << "SNOWFLAKE_WIF_ATTESTATION_TEST_RESOURCE is not set" << std::endl;
    return;
  }

  AttestationConfig config;
  config.type = AttestationType::AZURE;
  config.snowflakeEntraResource = resource;
  auto attestationOpt = Snowflake::Client::createAttestation(config);
  assert_true(attestationOpt.has_value());
  auto& attestation = attestationOpt.value();
  assert_true(attestation.type == Snowflake::Client::AttestationType::AZURE);
  assert_true(!attestation.credential.empty());
  std::cerr << "Credential: " << attestation.credential << std::endl;
  assert_true(!attestation.issuer.empty());
  std::cerr << "Issuer: " << attestation.credential << std::endl;
  assert_true(!attestation.subject.empty());
  std::cerr << "Subject: " << attestation.subject << std::endl;
}

int main()
{
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_aws_attestation),
      cmocka_unit_test(test_gcp_attestation),
      cmocka_unit_test(test_azure_attestation)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
