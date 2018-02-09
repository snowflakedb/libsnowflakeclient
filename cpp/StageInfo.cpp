//
// Created by hyu on 2/7/18.
//

#include "StageInfo.hpp"
#include "string.h"

using namespace Snowflake::Client;

StageInfo::StageInfo(SF_STAGE_INFO *stage_info) :
  m_location(stage_info->location),
  m_path(stage_info->path),
  m_region(stage_info->region),
  m_credentials{
    {"AWS_KEY_ID", stage_info->stage_cred->aws_key_id},
    {"AWS_SECRET_KEY", stage_info->stage_cred->aws_secret_key},
    {"AWS_TOKEN", stage_info->stage_cred->aws_token}
  }
{
  if (strcmp(stage_info->location_type, "s3") == 0)
  {
    m_stageType = S3;
  }
  else if (strcmp(stage_info->location_type, "azure") == 0)
  {
    m_stageType = AZURE;
  }
  else if (strcmp(stage_info->location_type, "local_fs") == 0)
  {
    m_stageType = LOCAL_FS;
  }
}