//
// Created by hyu on 2/5/18.
//

#include "FileTransferAgent.hpp"
#include "iostream";

using namespace Snowflake::Client;

FileTransferAgent::FileTransferAgent(IStatementPutGet *statement) :
  m_stmtPutGet(statement)
{
}

FileTransferResult* FileTransferAgent::execute(std::string *command)
{
  PutGetParseResponse *response = m_stmtPutGet->parsePutGetCommand(command);
  std::cout << "Command: " << * (response->getCommand()) << std::endl;
  std::cout << "Compression: " << * (response->getSourceCompression()) << std::endl;
  delete response;

  //TODO do upload and download accordingly

  return nullptr;
}