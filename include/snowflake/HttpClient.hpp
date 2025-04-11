
#ifndef SNOWFLAKECLIENT_HTTPCLIENT_HPP
#define SNOWFLAKECLIENT_HTTPCLIENT_HPP

#include <string>
#include <map>

#include <boost/url/url.hpp>

namespace Snowflake {
  namespace Client {
    struct HttpResponse {
      long code;

      std::string getBody() const {
        return std::string(m_buffer.begin(), m_buffer.end());
      }

      std::vector<char> m_buffer;
    };

    struct HttpRequest {
      enum class Method {
        GET,
        PUT,
        DELETE,
        POST,
      } method;

      boost::urls::url url;
      std::map <std::string, std::string> headers;
    };

    class IHttpClient {
    public:
      virtual HttpResponse run(HttpRequest req) = 0;
      virtual ~IHttpClient() = default;

      static IHttpClient* createSimple();
    };

  }
}
#endif //SNOWFLAKECLIENT_HTTPCLIENT_HPP
