#include <unistd.h>
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
  std::vector<std::string> detectedPlatforms;
  PlatformDetection::getDetectedPlatforms(detectedPlatforms);
  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), expectedPlatform.c_str());
  restoreMetadataBaseUrl();
}

void test_ec2Instance(void**) {
  test_detection_endpoint_core("is_ec2_instance", "aws_ec2_instance_success.json");
}

} // namespace Snowflake::Client

using namespace Snowflake::Client;
int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_ec2Instance),
  };
  return cmocka_run_group_tests(tests, setup_wiremock, teardown_wiremock);
}

