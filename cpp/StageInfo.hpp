//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_STAGEINFO_HPP
#define SNOWFLAKECLIENT_STAGEINFO_HPP

#include <string>
#include <unordered_map>
#include <client_int.h>

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

    class StageInfo
    {
    public:
      StageInfo(SF_STAGE_INFO *stage_info);

    private:
      StageType m_stageType;

      char* m_location;

      char* m_path;

      // required by s3 client
      char* m_region;

      std::unordered_map<std::string, char *> m_credentials;

    };
  }
}

#endif //SNOWFLAKECLIENT_STAGEINFO_HPP
