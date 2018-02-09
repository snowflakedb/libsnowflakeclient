//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP
#define SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP

#include <string>

namespace Snowflake
{
  namespace Client
  {
    class EncryptionMaterial
    {
    public:
      EncryptionMaterial(char *queryStageMasterKey,
                         char *queryId,
                         long smkId) :
        m_queryStageMasterKey(queryStageMasterKey),
        m_queryId(queryId),
        m_smkId(smkId)
      {
      };

      inline char* getQueryStageMasterKey()
      {
        return m_queryStageMasterKey;
      }

      inline char* getQueryId()
      {
        return m_queryId;
      }

      inline long getSmkId()
      {
        return m_smkId;
      }

    private:
      char *m_queryStageMasterKey;
      char *m_queryId;
      long m_smkId;
    };
  }
}

#endif //SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP
