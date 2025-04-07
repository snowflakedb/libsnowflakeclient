
#include "snowflake/HttpClient.hpp"
#include <curl/curl.h>

namespace Snowflake {
  namespace Client {

    class SimpleHttpClient : public IHttpClient {
    public:
      HttpResponse run(HttpRequest req) override {
        HttpResponse response;
        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
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
        }
        response.code = response_code;

        if (header_list) {
          curl_slist_free_all(header_list);
        }
        curl_easy_cleanup(curl);
        return response;
      }

    private:
      static void write(void *ptr, size_t size, size_t nmemb, HttpResponse *response) {
        response->m_buffer.insert(response->m_buffer.end(), (char *) ptr, (char *) ptr + size * nmemb);
      }
    };

    IHttpClient *IHttpClient::createSimple() {
      return new SimpleHttpClient();
    }
  }
}
