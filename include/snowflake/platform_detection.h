#ifndef SNOWFLAKE_PLATFORM_DETECTION_H
#define SNOWFLAKE_PLATFORM_DETECTION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

  // Platforms that can be detected using environment variable
#define IS_AWS_LAMBDA             "is_aws_lambda"
#define IS_AZURE_FUNCTION         "is_azure_function"
#define IS_GCE_CLOUD_RUN_SERVICE  "is_gce_cloud_run_service"
#define IS_GCE_CLOUD_RUN_JOB      "is_gce_cloud_run_job"
#define IS_GITHUB_ACTION          "is_github_action"
#define DISABLED                  "disabled"

  typedef enum platformDetectionState
  {
    platformDetected = 0,
    platformNotDetected = 1,
    platformDetectionTimeout = 2
  } platformDetectionState;

  typedef platformDetectionState (*detector_function_type)(void);

  struct detectorFunc
  {
    const char* name;
    detector_function_type fn;
  };

#define PLATFORM_ROWS 6
#define PLATFORM_COLS 50

  typedef struct detected_platforms
  {
    char names[PLATFORM_ROWS][PLATFORM_COLS];
    int count;
  } detected_platforms;

  // Detect platforms using environment variable checks
  detected_platforms detect_platforms(void);

#ifdef __cplusplus
}
#endif

#endif
