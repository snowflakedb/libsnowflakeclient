
#include "snowflake/HttpClient.hpp"
#include "../logger/SFLogger.hpp"
#include "constants.h"
#include "openssl/ssl.h"
#include <curl/curl.h>

namespace Snowflake {
  namespace Client {
    const HttpClientConfig defaultHttpClientConfig = {
      5, // config timeout in seconds
      0, // not to use other timeout by default
      0,
      0
    };

    class SimpleHttpClient : public IHttpClient {
    public:
      explicit SimpleHttpClient(const HttpClientConfig& cfg) : config(cfg), m_curl(NULL) {}
      boost::optional<HttpResponse> run(HttpRequest req) override {
        CURL *curl = curl_easy_init();
        m_curl = curl;
        HttpResponse response;
        boost::optional<HttpResponse> responseOpt = boost::none;

        if (config.connectTimeoutInMilliSeconds > 0) {
          curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, config.connectTimeoutInMilliSeconds);
        } else {
          curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, config.connectTimeoutInSeconds);
        }
        if (config.requestTimeoutInSeconds > 0)
        {
          curl_easy_setopt(curl, CURLOPT_TIMEOUT, config.requestTimeoutInSeconds);
        }
        else if (config.requestTimeoutInMilliSeconds > 0)
        {
          curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config.requestTimeoutInMilliSeconds);
        }
        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, HttpRequest::methodToString(req.method));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SimpleHttpClient::write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &response);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, SimpleHttpClient::writeheader);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*) &response);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, (long)SSL_VERSION);

        if (!req.body.empty()) {
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
          curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.body.size());
        }

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
        m_curl = NULL;
        return responseOpt;
      }

      std::string getNegotiatedTLSVersion() override
      {
        return m_negotiatedTLSVersion;
      }

    private:
      static size_t write(void *ptr, size_t size, size_t nmemb, HttpResponse *response) {
        CXX_LOG_TRACE("Writing %d bytes", (int) (size * nmemb));
        response->buffer.insert(response->buffer.end(), (char *) ptr, (char *) ptr + size * nmemb);
        return size * nmemb;
      }

      static size_t writeheader(void *ptr, size_t size, size_t nmemb, HttpResponse *response) {
        CXX_LOG_TRACE("Writing header %d bytes", (int) (size * nmemb));
        response->headerBuffer.insert(response->headerBuffer.end(), (char *) ptr, (char *) ptr + size * nmemb);
        return size * nmemb;
      }

      void updateNegotiatedTLSVersion()
      {
          m_negotiatedTLSVersion = "";
          if (!m_curl)
          {
              return;
          }
          const struct curl_tlssessioninfo* info = nullptr;
          CURLcode info_res = curl_easy_getinfo(m_curl, CURLINFO_TLS_SSL_PTR, &info);

          if ((info_res != CURLE_OK) || !info ||
              (info->backend != CURLSSLBACKEND_OPENSSL) ||
              !info->internals)
          {
              CXX_LOG_DEBUG("SimpleHttpClient::updateNegotiatedTLSVersion: negotiated TLS version info not available %d, %p",
                  info_res, info);
              return;
          }
          // Cast internals to OpenSSL's SSL structure
          SSL* ssl_con = static_cast<SSL*>(info->internals);
          const char* ssl_version = SSL_get_version(ssl_con);
          if (!ssl_version)
          {
              return;
          }

          m_negotiatedTLSVersion = ssl_version;
          CXX_LOG_DEBUG("SimpleHttpClient::updateNegotiatedTLSVersion: negotiated TLS version %s",
              m_negotiatedTLSVersion.c_str());
      }

      static int prereqCallback(void* clientp,
                                char*, char*, int, int)
      {
        SimpleHttpClient* client = (SimpleHttpClient*)clientp;
        if (client)
        {
            client->updateNegotiatedTLSVersion();
        }
        return CURL_PREREQFUNC_OK;
      }

      HttpClientConfig config;
      // for collecting negotiated TLS version
      CURL* m_curl;
      std::string m_negotiatedTLSVersion;

    };

    IHttpClient *IHttpClient::createSimple(const HttpClientConfig& cfg) {
      return new SimpleHttpClient(cfg);
    }

    IHttpClient *IHttpClient::getInstance() {
      static std::unique_ptr<IHttpClient> instance = std::make_unique<SimpleHttpClient>(defaultHttpClientConfig);
      return instance.get();
    }
  }
}
