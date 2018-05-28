/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_STAGEINFO_HPP
#define SNOWFLAKECLIENT_STAGEINFO_HPP

#include <string>
#include <unordered_map>
#include "client_int.h"

namespace Snowflake
{
namespace Client
{
enum StageType
{
  S3,
  AZURE,
  LOCAL_FS,

  /// This is used to create MOCKED storage client and is for testing purpose
  MOCKED_STAGE_TYPE,
};

/**
 * Remote storage info. (Region, Stage credentials etc)
 */
class StageInfo
{
public:
  StageInfo() {}

  StageInfo(SF_STAGE_INFO *stage_info);

  inline StageType getStageType()
  {
    return m_stageType;
  }

  inline std::string *getRegion()
  {
    return &m_region;
  }

  inline std::unordered_map<std::string, char *> *getCredentials()
  {
    return &m_credentials;
  }

  inline std::string *getLocation()
  {
    return &m_location;
  }

  inline void SetStageType(StageType stageType)
  {
    m_stageType = stageType;
  }

  inline void SetRegion(std::string &region)
  {
    m_region = region;
  }

  inline void SetLocation(std::string &location)
  {
    m_location = location;
  }

  inline void SetPath(std::string &path)
  {
    m_path =  path;
  }

  inline void SetCredentials(std::unordered_map<std::string, char *> &credentials)
  {
    m_credentials = credentials;
  }

private:
  StageType m_stageType;

  std::string m_location;

  std::string m_path;

  // required by s3 client
  std::string m_region;

  std::unordered_map<std::string, char *> m_credentials;

};
}
}

#endif //SNOWFLAKECLIENT_STAGEINFO_HPP
