
#ifndef SNOWFLAKECLIENT_HTTPCLIENT_HPP
#define SNOWFLAKECLIENT_HTTPCLIENT_HPP

#include <string>
#include <map>

#include <boost/url/url.hpp>
#include <boost/optional.hpp>

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

      static std::string methodToString(Method method) {
        switch (method) {
          case Method::GET:
            return "GET";
          case Method::PUT:
            return "PUT";
          case Method::DELETE:
            return "DELETE";
          case Method::POST:
            return "POST";
        }
        return "";
      }

      boost::urls::url url;
      std::map <std::string, std::string> headers;
    };

    class IHttpClient {
    public:
      virtual boost::optional<HttpResponse> run(HttpRequest req) = 0;
      virtual ~IHttpClient() = default;

      static IHttpClient* createSimple();
    };

  }
}
#endif //SNOWFLAKECLIENT_HTTPCLIENT_HPP
