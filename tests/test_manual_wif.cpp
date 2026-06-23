#include <cstring>
#include <iostream>
#include <memory>
#include <openssl/rsa.h>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "snowflake/WifAttestation.hpp"
#include "snowflake/client.h"

using namespace Snowflake::Client;

void test_wif_attestation_config(void**)
{
    AttestationConfig config;
    SF_CONNECT* conn = snowflake_init();
    snowflake_set_attribute(conn, SF_CON_WIF_AZURE_RESOURCE, "dummy_resource");
    config.configureWIFAttestation(conn);

    assert_false(config.audience.has_value());
    assert_false(config.snowflakeEntraResource.has_value());

    snowflake_set_attribute(conn, SF_CON_WIF_PROVIDER, "AWS");
    config.configureWIFAttestation(conn);

    assert_true(config.type.has_value());
    assert_int_equal(config.type.get(), AttestationType::AWS);

    assert_true(config.audience.has_value());
    assert_string_equal(config.audience.get().c_str(), SF_SNOWFLAKE_WIF_AUDIENCE);

    assert_true(config.snowflakeEntraResource.has_value());
    assert_string_equal(config.snowflakeEntraResource.get().c_str(), "dummy_resource");

    snowflake_set_attribute(conn, SF_CON_WIF_PROVIDER, "GCP");
    snowflake_set_attribute(conn, SF_CON_WIF_AUDIENCE, "dummy_audience.com");
    snowflake_set_attribute(conn, SF_CON_WORKLOAD_IDENTITY_IMPERSONATION_PATH, "dummy_impersonation_path");
    snowflake_set_attribute(conn, SF_CON_WIF_TOKEN, "dummy_token");

    config.configureWIFAttestation(conn);

    assert_true(config.type.has_value());
    assert_int_equal(config.type.get(), AttestationType::GCP);

    assert_true(config.workloadIdentityImpersonationPath.has_value());
    assert_string_equal(config.workloadIdentityImpersonationPath.get().c_str(), "dummy_impersonation_path");

    assert_true(config.token.has_value());
    assert_string_equal(config.token.get().c_str(), "dummy_token");

    assert_true(config.audience.has_value());
    assert_string_equal(config.audience.get().c_str(), "dummy_audience.com");

    assert_true(config.snowflakeEntraResource.has_value());
    assert_string_equal(config.snowflakeEntraResource.get().c_str(), "dummy_resource");
}


// AWS attestation is exercised end-to-end through snowflake_connect() in
// tests/test_auth/test_wif.c::test_aws_wif_authentication (gated on the same
// SNOWFLAKE_WIF_ATTESTATION_TEST_TYPE=AWS env var). The credential format is
// now a raw JWT, so the legacy "base64-decode + re-POST" reconstruction
// pattern below no longer applies; the auth-suite cloud test is the canonical
// AWS WIF runtime check.

void test_gcp_attestation(void**)
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
  assert_true(attestation.subject.has_value());
  std::cerr << "Subject: " << attestation.subject.get() << std::endl;
  assert_true(!attestation.subject->empty());
}

void test_azure_attestation(void**)
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
  assert_true(attestation.issuer.has_value());
  assert_true(!attestation.issuer->empty());
  std::cerr << "Issuer: " << attestation.credential << std::endl;
  assert_true(!attestation.subject->empty());
  assert_true(attestation.subject.has_value());
  std::cerr << "Subject: " << attestation.subject.get() << std::endl;
}

int main()
{
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_wif_attestation_config),
      cmocka_unit_test(test_gcp_attestation),
      cmocka_unit_test(test_azure_attestation)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
