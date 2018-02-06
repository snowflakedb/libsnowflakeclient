//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_STAGEINFO_HPP
#define SNOWFLAKECLIENT_STAGEINFO_HPP

namespace Snowflake
{
  namespace Client
  {
    enum StageType
    {
      S3,
      AZURE
    };

    class StageInfo
    {
    public:
      StageInfo(StageType * stageType);

    private:
      StageType* m_stageType;

      std::string * m_location;

      std::string * m_path;

      // required by s3 client
      std::string * m_region;

      std::unordered_map<std::string, std::string> *credentials;

    };
  }
}

#endif //SNOWFLAKECLIENT_STAGEINFO_HPP
