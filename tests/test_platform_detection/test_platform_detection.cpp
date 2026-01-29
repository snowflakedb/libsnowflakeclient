#include <unistd.h>
#include <algorithm>
#include <chrono>
#include "snowflake/PlatformDetection.hpp"
#include "snowflake/platform.h"
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

  std::string wiremockUrl = std::string("http://") + wiremockHost + ":" + wiremockAdminPort;
  redirectMetadataBaseUrl(wiremockUrl.c_str());

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
  restoreMetadataBaseUrl();
  return 0;
}

void test_detection_endpoint_core(const std::string& expectedPlatform, const std::string& mappingFile)
{
  resetDetection();
  WiremockRunner::resetMapping();
  WiremockRunner::initMappingFromFile(mappingFile);
  // this doesn't work likely due to the AWS SDK version we are using.
  // Didn't find AWS_ENDPOINT_URL* in the source code.
  // no test case for has_aws_identity for now, while it's confirmed working on AWS instance.
  std::string wiremockUrl = std::string("http://") + wiremockHost + ":" + wiremockAdminPort;
  sf_setenv("AWS_ENDPOINT_URL_STS", wiremockUrl.c_str());
  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);
  // On aws instance has_aws_identity is also returned
  assert_true(detectedPlatforms.size() <= 2);
  assert_true(std::find(detectedPlatforms.begin(), detectedPlatforms.end(), expectedPlatform) != detectedPlatforms.end());
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

void test_disablePlatformDetection(void**) {
  resetDetection();
  sf_setenv("SNOWFLAKE_DISABLE_PLATFORM_DETECTION", "true");

  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);

  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), "disabled");

  sf_unsetenv("SNOWFLAKE_DISABLE_PLATFORM_DETECTION");
}

void test_awsLambdaEnv(void**) {
  resetDetection();
  sf_setenv("LAMBDA_TASK_ROOT", "/var/task");

  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);

  // On aws instance has_aws_identity is also returned
  assert_true(detectedPlatforms.size() <= 2);
  assert_true(std::find(detectedPlatforms.begin(), detectedPlatforms.end(), "is_aws_lambda") != detectedPlatforms.end());

  sf_unsetenv("LAMBDA_TASK_ROOT");
}

void test_azureFunctionEnv(void**) {
  resetDetection();
  sf_setenv("FUNCTIONS_WORKER_RUNTIME", "node");
  sf_setenv("FUNCTIONS_EXTENSION_VERSION", "~4");
  sf_setenv("AzureWebJobsStorage", "DefaultEndpointsProtocol=https;AccountName=test");

  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);

  // On aws instance has_aws_identity is also returned
  assert_true(detectedPlatforms.size() <= 2);
  assert_true(std::find(detectedPlatforms.begin(), detectedPlatforms.end(), "is_azure_function") != detectedPlatforms.end());

  sf_unsetenv("FUNCTIONS_WORKER_RUNTIME");
  sf_unsetenv("FUNCTIONS_EXTENSION_VERSION");
  sf_unsetenv("AzureWebJobsStorage");
}

void test_gceCloudRunServiceEnv(void**) {
  resetDetection();
  sf_setenv("K_SERVICE", "my-service");
  sf_setenv("K_REVISION", "my-service-00001");
  sf_setenv("K_CONFIGURATION", "my-service");

  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);

  // On aws instance has_aws_identity is also returned
  assert_true(detectedPlatforms.size() <= 2);
  assert_true(std::find(detectedPlatforms.begin(), detectedPlatforms.end(), "is_gce_cloud_run_service") != detectedPlatforms.end());

  sf_unsetenv("K_SERVICE");
  sf_unsetenv("K_REVISION");
  sf_unsetenv("K_CONFIGURATION");
}

void test_gceCloudRunJobEnv(void**) {
  resetDetection();
  sf_setenv("CLOUD_RUN_JOB", "my-job");
  sf_setenv("CLOUD_RUN_EXECUTION", "my-job-execution-1");

  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);

  // On aws instance has_aws_identity is also returned
  assert_true(detectedPlatforms.size() <= 2);
  assert_true(std::find(detectedPlatforms.begin(), detectedPlatforms.end(), "is_gce_cloud_run_job") != detectedPlatforms.end());

  sf_unsetenv("CLOUD_RUN_JOB");
  sf_unsetenv("CLOUD_RUN_EXECUTION");
}

void test_githubActionEnv(void**) {
  resetDetection();
  sf_setenv("GITHUB_ACTIONS", "true");

  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);

  // On aws instance has_aws_identity is also returned
  assert_true(detectedPlatforms.size() <= 2);
  assert_true(std::find(detectedPlatforms.begin(), detectedPlatforms.end(), "is_github_action") != detectedPlatforms.end());

  sf_unsetenv("GITHUB_ACTIONS");
}

void test_timeout(void**) {
  resetDetection();
  WiremockRunner::resetMapping();
  WiremockRunner::initMappingFromFile("timeout_response.json");
  std::string wiremockUrl = std::string("http://") + wiremockHost + ":" + wiremockAdminPort;
  sf_setenv("AWS_ENDPOINT_URL_STS", wiremockUrl.c_str());
  std::vector<std::string> detectedPlatforms;
  auto startTime = std::chrono::steady_clock::now();
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);
  auto endTime = std::chrono::steady_clock::now();
  auto execTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  assert_true(execTimeMs >= 200);
  assert_true(execTimeMs <= 250);
  sf_unsetenv("AWS_ENDPOINT_URL_STS");
}

} // namespace Snowflake::Client

using namespace Snowflake::Client;
int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_disablePlatformDetection),
      cmocka_unit_test(test_awsLambdaEnv),
      cmocka_unit_test(test_azureFunctionEnv),
      cmocka_unit_test(test_gceCloudRunServiceEnv),
      cmocka_unit_test(test_gceCloudRunJobEnv),
      cmocka_unit_test(test_githubActionEnv),
      cmocka_unit_test(test_ec2Instance),
      cmocka_unit_test(test_azurevm),
      cmocka_unit_test(test_azureIdentity),
      cmocka_unit_test(test_gcevm),
      cmocka_unit_test(test_gcpIdentity),
  };
  return cmocka_run_group_tests(tests, setup_wiremock, teardown_wiremock);
}
