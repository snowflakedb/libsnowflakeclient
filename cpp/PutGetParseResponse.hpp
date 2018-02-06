//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_PUTGETPARSERESPONSE_HPP
#define SNOWFLAKECLIENT_PUTGETPARSERESPONSE_HPP

namespace Snowflake
{
  namespace Client
  {
    class PutGetParseResponse
    {
    private:
      UploadInfo * m_uploadInfo;

      std::vector<std::string> * m_srcLocations;

      int m_parallel;

      bool autoCompress;

      bool overwrite;

      std::string *m_sourceCompression;

      bool m_clientShowEncryptionParameter;

      EncryptionMaterial * encryptionMaterial;

      StageInfo * stageInfo;

      std::string command;
    };
  }
}


#endif //SNOWFLAKECLIENT_PUTGETPARSERESPONSE_HPP
