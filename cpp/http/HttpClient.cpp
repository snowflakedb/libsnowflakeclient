
#include "snowflake/HttpClient.hpp"
#include "../logger/SFLogger.hpp"
#include <curl/curl.h>

namespace Snowflake {
  namespace Client {
    const HttpClientConfig defaultHttpClientConfig = {
      5, // config timeout in seconds
    };

    class SimpleHttpClient : public IHttpClient {
    public:
      explicit SimpleHttpClient(const HttpClientConfig& cfg) : config(cfg) {}
      boost::optional<HttpResponse> run(HttpRequest req) override {
        CURL *curl = curl_easy_init();
        HttpResponse response;
        boost::optional<HttpResponse> responseOpt = boost::none;

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, config.connectTimeoutInSeconds);
        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, HttpRequest::methodToString(req.method));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SimpleHttpClient::write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &response);

        struct curl_slist *header_list = nullptr;
        for (const auto &h: req.headers) {
          std::string hdr = h.first + ": " + h.second;
          header_list = curl_slist_append(header_list, hdr.c_str());
        }
        if (header_list) {
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }

        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        if (res == CURLE_OK) {
          curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
          response.code = response_code;
          responseOpt = response;
        }
        else {
          CXX_LOG_ERROR("Curl error: %s", curl_easy_strerror(res));
        }

        if (header_list) {
          curl_slist_free_all(header_list);
        }
        curl_easy_cleanup(curl);
        return responseOpt;
      }

    private:
      static size_t write(void *ptr, size_t size, size_t nmemb, HttpResponse *response) {
        CXX_LOG_TRACE("Writing %d bytes", (int) (size * nmemb));
        response->buffer.insert(response->buffer.end(), (char *) ptr, (char *) ptr + size * nmemb);
        return size * nmemb;
      }

      HttpClientConfig config;
    };

    IHttpClient *IHttpClient::createSimple(const HttpClientConfig& cfg) {
      return new SimpleHttpClient(cfg);
    }
  }
}
