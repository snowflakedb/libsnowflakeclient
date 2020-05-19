/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_STATEMENTPUTGET_HPP
#define SNOWFLAKECLIENT_STATEMENTPUTGET_HPP

#include "snowflake/IStatementPutGet.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include "snowflake/client.h"
#include "connection.h"
#include <curl/curl.h>

namespace Snowflake
{
namespace Client
{

/**
 * C API implementation of IStatementPutGet.
 * Communicate with server to parse statement
 */
class StatementPutGet : public Snowflake::Client::IStatementPutGet
{
public:
  StatementPutGet(SF_STMT *stmt);

  virtual bool parsePutGetCommand(std::string *sql,
                                  PutGetParseResponse *putGetParseResponse);

  /**
  * PUT/GET on GCS use this interface to perform put request.
  * Not implemented by default.
  * @param url The url of the request.
  * @param headers The headers of the request.
  * @param payload The upload data.
  * @param responseHeaders The headers of the response.
  *
  * return true if succeed otherwise false
  */
  virtual bool http_put(std::string const& url,
    std::vector<std::string> const& headers,
    std::basic_iostream<char>& payload,
    size_t payloadLen,
    std::string& responseHeaders) override;

  /**
  * PUT/GET on GCS use this interface to perform put request.
  * Not implemented by default.
  * @param url The url of the request.
  * @param headers The headers of the request.
  * @param payload The upload data.
  * @param responseHeaders The headers of the response.
  * @param headerOnly True if get response header only without payload body.
  *
  * return true if succeed otherwise false
  */
  virtual bool http_get(std::string const& url,
    std::vector<std::string> const& headers,
    std::basic_iostream<char>* payload,
    std::string& responseHeaders,
    bool headerOnly) override;

private:
  /**
  * Performs an HTTP request with retry. For now, support put/get only.
  *
  * @param curl The cURL object to use in the request.
  * @param request_type The type of HTTP request.
  * @param url The fully qualified URL to use for the HTTP request.
  * @param header The header to use for the HTTP request.
  *        Could be NULL for get request if only headers of response is needed.
  * @param payload The payload stream for put/get HTTP request. Could be NULL
  *        retrieve response header only.
  * @param payloadLen The data length of the payload stream, for put request only.
  * @param responseHeaders The headers in response.
  * @param headerOnly True if get response header only without payload body.
  * @return Success/failure status of http request call. true = Success; false = Failure
  */
  bool http_perform(SF_REQUEST_TYPE request_type, std::string const& url,
                    std::vector<std::string> const& headers,
                    std::basic_iostream<char>* payload,
                    size_t payloadLen,
                    std::string& responseHeaders,
                    bool headerOnly = false);

  SF_STMT *m_stmt;
};
}
}


#endif //SNOWFLAKECLIENT_STATEMENTPUTGET_HPP
