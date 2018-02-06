//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP
#define SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP

#include "string"

namespace Snowflake
{
  namespace Client
  {
    class RemoteStoreFileEncryptionMaterial
    {
    public:
      RemoteStoreFileEncryptionMaterial(std::string *queryStageMasterKey,
                                        std::string *queryId,
                                        long *smkId) :
        m_queryStageMasterKey(queryStageMasterKey),
        m_queryId(queryId),
        m_smkId(smkId)
      {
      };

      inline std::string* getQueryStageMasterKey()
      {
        return m_queryStageMasterKey;
      }

      inline std::string* getQueryId()
      {
        return m_queryId;
      }

      inline long* getSmkId()
      {
        return m_smkId;
      }

    private:
      std::string *m_queryStageMasterKey;
      std::string *m_queryId;
      long *m_smkId;
    };
  }
}

#endif //SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP
