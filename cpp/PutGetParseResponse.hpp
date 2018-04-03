/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

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
enum CommandType
{
  UPLOAD, DOWNLOAD, UNKNOWN
};

/**
 * PUT/GET command response from server.
 */
class PutGetParseResponse
{
public:
  PutGetParseResponse() {};

  PutGetParseResponse(SF_PUT_GET_RESPONSE *put_get_response);

  void updateWith(SF_PUT_GET_RESPONSE *put_get_response);

  ~PutGetParseResponse() {};

  inline CommandType getCommand()
  {
    return m_command;
  }

  inline char * getSourceCompression()
  {
    return m_sourceCompression;
  }

  inline EncryptionMaterial *getEncryptionMaterial()
  {
    return &m_encryptionMaterial;
  }

  inline std::vector<std::string> *getSourceLocations()
  {
    return &m_srcLocations;
  }

  inline StageInfo *getStageInfo()
  {
    return &m_stageInfo;
  }

  inline bool getAutoCompress()
  {
    return m_autoCompress;
  }

  inline int getParallel()
  {
    return m_parallel;
  }

private:

  int m_parallel;

  bool m_autoCompress;

  bool m_overwrite;

  bool m_clientShowEncryptionParameter;

  char* m_sourceCompression;

  CommandType m_command;

  std::vector<std::string> m_srcLocations;

  EncryptionMaterial m_encryptionMaterial;

  StageInfo m_stageInfo;
};

}
}


#endif //SNOWFLAKECLIENT_PUTGETPARSERESPONSE_HPP
