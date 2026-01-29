#include <unistd.h>
#include <algorithm>
#include "snowflake/PlatformDetection.hpp"
#include "../utils/test_setup.h"
#include "../utils/TestSetup.hpp"
#include "../wiremock/wiremock.hpp"

extern "C"
{
extern void resetDetection();
extern void redirectMetadataBaseUrl(const char* url);
extern void restoreMetadataBaseUrl();
}

namespace Snowflake::Client {

WiremockRunner* wiremock = NULL;
// Check if WireMock is running by polling the admin endpoint
static bool isWiremockRunning() {
  const std::string request = std::string("curl -s --output /dev/null -X POST http://") +
                              wiremockHost + ":" + wiremockAdminPort + "/__admin/mappings ";
  const int ret = std::system(request.c_str());
  return ret == 0;
}

int setup_wiremock(void **) {
  wiremock = new WiremockRunner();

  snowflake_global_set_attribute(SF_GLOBAL_DISABLE_VERIFY_PEER, &SF_BOOLEAN_TRUE);
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_FALSE);

  // Wait for WireMock to be ready
  const int max_attempts = 10;
  int attempts = 0;
  while (!isWiremockRunning() && attempts < max_attempts) {
    sleep(1);
    attempts++;
  }

  if (!isWiremockRunning()) {
    return -1;
  }

  return 0;
}

int teardown_wiremock(void **) {
  if (wiremock) {
    delete wiremock;
    wiremock = nullptr;
  }
  return 0;
}

void test_detection_endpoint_core(const std::string& expectedPlatform, const std::string& mappingFile)
{
  resetDetection();
  WiremockRunner::resetMapping();
  WiremockRunner::initMappingFromFile(mappingFile);
  std::string wiremockUrl = std::string("http://") + wiremockHost + ":" + wiremockAdminPort;
  redirectMetadataBaseUrl(wiremockUrl.c_str());
  // this doesn't work likely due to the AWS SDK version we are using.
  // Didn't find AWS_ENDPOINT_URL* in the source code.
  // no test case for has_aws_identity for now, while it's confirmed working on AWS instance.
  sf_setenv("AWS_ENDPOINT_URL_STS", wiremockUrl.c_str());
  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);
  // On aws instance has_aws_identity is also returned
  assert_true(detectedPlatforms.size() <= 2);
  assert_true(std::find(detectedPlatforms.begin(), detectedPlatforms.end(), expectedPlatform) != detectedPlatforms.end());
  restoreMetadataBaseUrl();
  sf_unsetenv("AWS_ENDPOINT_URL_STS");
}

void test_ec2Instance(void**) {
  test_detection_endpoint_core("is_ec2_instance", "aws_ec2_instance_success.json");
}

void test_azurevm(void**) {
  test_detection_endpoint_core("is_azure_vm", "azure_vm_success.json");
}

void test_azureIdentity(void**) {
  test_detection_endpoint_core("has_azure_managed_identity", "azure_managed_identity_success.json");
}

void test_gcevm(void**) {
  test_detection_endpoint_core("is_gce_vm", "gce_vm_success.json");
}

void test_gcpIdentity(void**) {
  test_detection_endpoint_core("has_gcp_identity", "gce_identity_success.json");
}

} // namespace Snowflake::Client

using namespace Snowflake::Client;
int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_ec2Instance),
      cmocka_unit_test(test_azurevm),
      cmocka_unit_test(test_azureIdentity),
      cmocka_unit_test(test_gcevm),
      cmocka_unit_test(test_gcpIdentity),
  };
  return cmocka_run_group_tests(tests, setup_wiremock, teardown_wiremock);
}

