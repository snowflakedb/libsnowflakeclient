#include "snowflake/HttpClient.hpp"
#include "../wiremock/wiremock.hpp"
#include <string>
#include <memory>
#include <unistd.h>
#include "../utils/test_setup.h"
#include "../utils/TestSetup.hpp"

namespace Snowflake::Client {
  WiremockRunner *wiremock = nullptr;

  static constexpr HttpClientConfig DEFAULT_CONFIG = {5};

  // Check if WireMock is running by polling the admin endpoint
  static bool isWiremockRunning() {
    const std::string request = std::string("curl -s --output /dev/null -X POST http://") + 
                                wiremockHost + ":" + wiremockAdminPort + "/__admin/mappings ";
    const int ret = std::system(request.c_str());
    return ret == 0;
  }

  int setup_wiremock(void **) {
    wiremock = new WiremockRunner();
    
    snowflake_global_set_attribute(SF_GLOBAL_DISABLE_VERIFY_PEER, &SF_BOOLEAN_TRUE);
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_FALSE);
    
    // Wait for WireMock to be ready
    const int max_attempts = 10;
    int attempts = 0;
    while (!isWiremockRunning() && attempts < max_attempts) {
      sleep(1);
      attempts++;
    }
    
    if (!isWiremockRunning()) {
      return -1;
    }
    
    return 0;
  }

  int teardown_wiremock(void **) {
    if (wiremock) {
      delete wiremock;
      wiremock = nullptr;
    }
    return 0;
  }

  void test_http_client_get_request(void **) {
    WiremockRunner::resetMapping();
    WiremockRunner::initMappingFromFile("get_request.json");

    const std::unique_ptr<IHttpClient> client(IHttpClient::createSimple(DEFAULT_CONFIG));

    auto url = boost::urls::url("http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/resource");
    url.params().append({"param1", "value1"});
    url.params().append({"param2", "value2"});

    HttpRequest request;
    request.method = HttpRequest::Method::GET;
    request.url = url;

    boost::optional<HttpResponse> response = client->run(request);

    assert_true(response.has_value());
    assert_int_equal(response->code, 200);

    const std::string body(response->buffer.data(), response->buffer.size());
    assert_true(body.find("success") != std::string::npos);
  }

  void test_http_client_post_with_body(void **) {
    WiremockRunner::resetMapping();
    WiremockRunner::initMappingFromFile("post_request.json");

    const std::unique_ptr<IHttpClient> client(IHttpClient::createSimple(DEFAULT_CONFIG));

    HttpRequest request;
    request.method = HttpRequest::Method::POST;
    request.url = boost::urls::url("http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/submit");
    request.headers["Content-Type"] = "application/json";
    request.body = R"({"key":"value","nested":{"field":"data"}})";

    boost::optional<HttpResponse> response = client->run(request);

    assert_true(response.has_value());
    assert_int_equal(response->code, 201);
  }

  void test_http_client_put_request(void **) {
    WiremockRunner::resetMapping();
    WiremockRunner::initMappingFromFile("put_request.json");

    const std::unique_ptr<IHttpClient> client(IHttpClient::createSimple(DEFAULT_CONFIG));

    HttpRequest request;
    request.method = HttpRequest::Method::PUT;
    request.url = boost::urls::url("http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/update");
    request.body = R"({"status":"updated"})";

    boost::optional<HttpResponse> response = client->run(request);

    assert_true(response.has_value());
    assert_int_equal(response->code, 200);
  }

  void test_http_client_delete_request(void **) {
    WiremockRunner::resetMapping();
    WiremockRunner::initMappingFromFile("delete_request.json");

    const std::unique_ptr<IHttpClient> client(IHttpClient::createSimple(DEFAULT_CONFIG));

    HttpRequest request;
    request.method = HttpRequest::Method::DEL;
    request.url = boost::urls::url("http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/resource/123");

    boost::optional<HttpResponse> response = client->run(request);

    assert_true(response.has_value());
    assert_int_equal(response->code, 204);
  }

  void test_http_client_custom_headers(void **) {
    WiremockRunner::resetMapping();
    WiremockRunner::initMappingFromFile("custom_headers.json");

    const std::unique_ptr<IHttpClient> client(IHttpClient::createSimple(DEFAULT_CONFIG));

    HttpRequest request;
    request.method = HttpRequest::Method::GET;
    request.url = boost::urls::url("http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/authenticated");
    request.headers["Authorization"] = "Bearer test-token-123";
    request.headers["X-Custom-Header"] = "custom-value";
    request.headers["X-Request-ID"] = "req-12345";

    boost::optional<HttpResponse> response = client->run(request);

    assert_true(response.has_value());
    assert_int_equal(response->code, 200);
  }

  void test_http_client_error_responses(void **) {
    WiremockRunner::resetMapping();
    WiremockRunner::initMappingFromFile("error_responses.json");

    std::unique_ptr<IHttpClient> client(IHttpClient::createSimple(DEFAULT_CONFIG));

    // Test 404
    HttpRequest request_404;
    request_404.method = HttpRequest::Method::GET;
    request_404.url = boost::urls::url("http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/notfound");

    boost::optional<HttpResponse> response_404 = client->run(request_404);
    assert_true(response_404.has_value());
    assert_int_equal(response_404->code, 404);

    // Test 500
    HttpRequest request_500;
    request_500.method = HttpRequest::Method::GET;
    request_500.url =
        boost::urls::url("http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/servererror");

    boost::optional<HttpResponse> response_500 = client->run(request_500);
    assert_true(response_500.has_value());
    assert_int_equal(response_500->code, 500);

    // Test 401
    HttpRequest request_401;
    request_401.method = HttpRequest::Method::GET;
    request_401.url = boost::urls::url(
      "http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/unauthorized");

    boost::optional<HttpResponse> response_401 = client->run(request_401);
    assert_true(response_401.has_value());
    assert_int_equal(response_401->code, 401);
  }

  void test_http_client_connection_failure(void **) {
    const std::unique_ptr<IHttpClient> client(IHttpClient::createSimple(DEFAULT_CONFIG));

    HttpRequest request;
    request.method = HttpRequest::Method::GET;
    request.url = boost::urls::url("http://invalid-host-12345.example.com/api/resource");

    const boost::optional<HttpResponse> response = client->run(request);

    // Should fail and return empty optional
    assert_false(response.has_value());
  }

  void test_http_client_large_response(void **) {
    WiremockRunner::resetMapping();
    WiremockRunner::initMappingFromFile("large_response.json");

    const std::unique_ptr<IHttpClient> client(IHttpClient::createSimple(DEFAULT_CONFIG));

    HttpRequest request;
    request.method = HttpRequest::Method::GET;
    request.url = boost::urls::url("http://" + std::string(wiremockHost) + ":" + std::string(wiremockAdminPort) + "/api/large");

    boost::optional<HttpResponse> response = client->run(request);

    assert_true(response.has_value());
    assert_int_equal(response->code, 200);
    // Response should be large (>1KB)
    assert_true(response->buffer.size() > 1024);
  }
}

int main() {
  using namespace Snowflake::Client;
  
  initialize_test(SF_BOOLEAN_FALSE);

    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_http_client_get_request),
      cmocka_unit_test(test_http_client_post_with_body),
      cmocka_unit_test(test_http_client_put_request),
      cmocka_unit_test(test_http_client_delete_request),
      cmocka_unit_test(test_http_client_custom_headers),
      cmocka_unit_test(test_http_client_error_responses),
      cmocka_unit_test(test_http_client_connection_failure),
      cmocka_unit_test(test_http_client_large_response),
    };

  const int ret = cmocka_run_group_tests(tests, setup_wiremock, teardown_wiremock);
  snowflake_global_term();
  return ret;
}
