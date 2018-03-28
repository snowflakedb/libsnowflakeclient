//
// Created by hyu on 2/5/18.
//

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
  LOCAL_FS
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
