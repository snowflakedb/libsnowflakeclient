//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_PUTGETPARSERESPONSE_HPP
#define SNOWFLAKECLIENT_PUTGETPARSERESPONSE_HPP

#include "string"
#include "EncryptionMaterial.hpp"
#include "StageInfo.hpp"
#include <vector>
#include "snowflake/client.h"

using namespace Snowflake::Client;

namespace Snowflake
{
  namespace Client
  {
    class PutGetParseResponse
    {
    public:
      PutGetParseResponse(SF_PUT_GET_RESPONSE *put_get_response);

      ~PutGetParseResponse();

      inline std::string *getCommand()
      {
        return &m_command;
      }

      inline std::string *getSourceCompression()
      {
        return &m_sourceCompression;
      }

      inline EncryptionMaterial *getEncryptionMaterial()
      {
        return m_encryptionMaterial;
      }

    private:

      int m_parallel;

      bool m_autoCompress;

      bool m_overwrite;

      bool m_clientShowEncryptionParameter;

      std::string m_sourceCompression;

      std::string m_command;

      std::vector<char *> *m_srcLocations;

      EncryptionMaterial * m_encryptionMaterial;

      StageInfo * m_stageInfo;
    };
  }
}


#endif //SNOWFLAKECLIENT_PUTGETPARSERESPONSE_HPP
