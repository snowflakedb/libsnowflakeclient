#include "snowflake/PlatformDetection.hpp"
#include "../utils/test_setup.h"
#include "../utils/TestSetup.hpp"
#include "../wiremock/wiremock.hpp"

using namespace Snowflake::Client;

Snowflake::Client::WiremockRunner* wiremock = NULL;

extern void resetDetection();
extern void redirectMetadataBaseUrl(const char* url);
extern void restoreMetadataBaseUrl();

void test_detection_endpoint_core(const std::string& expectedPlatform, const std::string& mappingFile)
{
  resetDetection();
  wiremock = new WiremockRunner(mappingFile, {});
  std::string wiremockUrl = std::string("http://") + wiremockHost + ":" + wiremockPort;
  redirectMetadataBaseUrl(wiremockUrl.c_str());
  std::vector<std::string> detectedPlatforms;
  getDetectedPlatforms(detectedPlatforms);
  assert_int_equal(1, detectedPlatforms.size());
  assert_string_equal(detectedPlatforms[0].c_str(), expectedPlatform.c_str());
  restoreMetadataBaseUrl();
  delete(wiremock);
}

void test_ec2Instance(void**) {
  test_detection_endpoint_core("is_ec2_instance", "aws_ec2_instance_success");
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_ec2Instance),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}

