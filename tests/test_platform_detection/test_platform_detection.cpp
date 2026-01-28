#include <unistd.h>
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
extern void sf_setenv();
extern void sf_unsetenv();
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
  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);
  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), expectedPlatform.c_str());
  restoreMetadataBaseUrl();
}

void test_ec2Instance(void**) {
  test_detection_endpoint_core("is_ec2_instance", "aws_ec2_instance_success.json");
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

  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), "is_aws_lambda");

  sf_unsetenv("LAMBDA_TASK_ROOT");
}

void test_azureFunctionEnv(void**) {
  resetDetection();
  sf_setenv("FUNCTIONS_WORKER_RUNTIME", "node");
  sf_setenv("FUNCTIONS_EXTENSION_VERSION", "~4");
  sf_setenv("AzureWebJobsStorage", "DefaultEndpointsProtocol=https;AccountName=test");

  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);

  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), "is_azure_function");

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

  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), "is_gce_cloud_run_service");

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

  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), "is_gce_cloud_run_job");

  sf_unsetenv("CLOUD_RUN_JOB");
  sf_unsetenv("CLOUD_RUN_EXECUTION");
}

void test_githubActionEnv(void**) {
  resetDetection();
  sf_setenv("GITHUB_ACTIONS", "true");

  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);

  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), "is_github_action");

  sf_unsetenv("GITHUB_ACTIONS");
}
} // namespace Snowflake::Client

using namespace Snowflake::Client;
int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_ec2Instance),
      cmocka_unit_test(test_disablePlatformDetection),
      cmocka_unit_test(test_awsLambdaEnv),
      cmocka_unit_test(test_azureFunctionEnv),
      cmocka_unit_test(test_gceCloudRunServiceEnv),
      cmocka_unit_test(test_gceCloudRunJobEnv),
      cmocka_unit_test(test_githubActionEnv),
  };
  return cmocka_run_group_tests(tests, setup_wiremock, teardown_wiremock);
}

