//
// Created by hyu on 2/5/18.
//

#include "FileTransferAgent.hpp"
#include "util/Base64.hpp"
#include <iostream>

using namespace Snowflake::Client;
using namespace Snowflake::Client::Util;

FileTransferAgent::FileTransferAgent(IStatementPutGet *statement) :
  m_stmtPutGet(statement)
{
}

FileTransferResult* FileTransferAgent::execute(std::string *command)
{
  PutGetParseResponse *response = m_stmtPutGet->parsePutGetCommand(command);
  std::cout << "Command: " << * (response->getCommand()) << std::endl;
  std::cout << "Compression: " << * (response->getSourceCompression()) << std::endl;

  std::string s = std::string(response->getEncryptionMaterial()->getQueryStageMasterKey());
  size_t decodeSize = Base64::decodedLength(s.c_str(), s.size());

  char * decode = new char[decodeSize];
  size_t len = Base64::decode(s.c_str(), s.size(), decode);

  std::cout << "Decode size " << len ;

  delete response;

  //TODO do upload and download accordingly

  return nullptr;
}