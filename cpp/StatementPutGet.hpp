

#ifndef SNOWFLAKECLIENT_STATEMENTPUTGET_HPP
#define SNOWFLAKECLIENT_STATEMENTPUTGET_HPP

#include "snowflake/IStatementPutGet.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include "snowflake/client.h"

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

      virtual Util::Proxy *get_proxy();

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
      virtual bool http_put(std::string const &url,
                            std::vector<std::string> const &headers,
                            std::basic_iostream<char> &payload,
                            size_t payloadLen,
                            std::string &responseHeaders);

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
      virtual bool http_get(std::string const &url,
                            std::vector<std::string> const &headers,
                            std::basic_iostream<char> *payload,
                            std::string &responseHeaders,
                            bool headerOnly);

    private:
      SF_STMT *m_stmt;
      Util::Proxy m_proxy;
      bool m_useProxy;
    };
  }
}

#endif // SNOWFLAKECLIENT_STATEMENTPUTGET_HPP
