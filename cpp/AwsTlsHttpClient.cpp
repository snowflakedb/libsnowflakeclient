#include "AwsTlsHttpClient.hpp"
#include "tls_config.h"
#include <snowflake/client.h>   // SF_TLS_VERSION_UNSET

#include <aws/core/http/standard/StandardHttpRequest.h>
#include <aws/core/http/URI.h>
#include <aws/core/utils/memory/AWSMemory.h>

namespace
{
  const char* SF_TLS_HTTP_TAG = "SFTlsHttpClient";

  // Construction-scoped carrier: set by ScopedAwsTlsVersion around the
  // synchronous `new S3Client(...)`, read by the factory's CreateHttpClient
  // (which runs on the same thread during that construction). Not request-level.
  thread_local long t_ctorTlsVersion = SF_TLS_VERSION_UNSET;
}

namespace Snowflake
{
namespace Client
{

TlsCurlHttpClient::TlsCurlHttpClient(const Aws::Client::ClientConfiguration& clientConfig,
                                     long tlsVersion)
  : Aws::Http::CurlHttpClient(clientConfig), m_tlsVersion(tlsVersion)
{}

void TlsCurlHttpClient::OverrideOptionsOnConnectionHandle(CURL* connectionHandle) const
{
  if (m_tlsVersion != SF_TLS_VERSION_UNSET)
  {
    // Shared helper: sets CURLOPT_SSLVERSION. Does not touch any other curl option.
    sf_apply_tls_version(connectionHandle, m_tlsVersion);
  }
}

std::shared_ptr<Aws::Http::HttpClient>
TlsCurlHttpClientFactory::CreateHttpClient(const Aws::Client::ClientConfiguration& clientConfiguration) const
{
  return Aws::MakeShared<TlsCurlHttpClient>(SF_TLS_HTTP_TAG, clientConfiguration, t_ctorTlsVersion);
}

std::shared_ptr<Aws::Http::HttpRequest>
TlsCurlHttpClientFactory::CreateHttpRequest(const Aws::String& uri, Aws::Http::HttpMethod method,
                                            const Aws::IOStreamFactory& streamFactory) const
{
  return CreateHttpRequest(Aws::Http::URI(uri), method, streamFactory);
}

std::shared_ptr<Aws::Http::HttpRequest>
TlsCurlHttpClientFactory::CreateHttpRequest(const Aws::Http::URI& uri, Aws::Http::HttpMethod method,
                                            const Aws::IOStreamFactory& streamFactory) const
{
  auto request = Aws::MakeShared<Aws::Http::Standard::StandardHttpRequest>(SF_TLS_HTTP_TAG, uri, method);
  request->SetResponseStreamFactory(streamFactory);
  return request;
}

void TlsCurlHttpClientFactory::InitStaticState()
{
  // Mirror DefaultHttpClientFactory. This repo builds the AWS SDK with the
  // default httpOptions.initAndCleanupCurl == true, so curl global state is
  // owned by the SDK; re-init it here after SetHttpClientFactory tore it down.
  Aws::Http::CurlHttpClient::InitGlobalState();
}

void TlsCurlHttpClientFactory::CleanupStaticState()
{
  Aws::Http::CurlHttpClient::CleanupGlobalState();
}

ScopedAwsTlsVersion::ScopedAwsTlsVersion(long tlsVersion)
{
  t_ctorTlsVersion = tlsVersion;
}

ScopedAwsTlsVersion::~ScopedAwsTlsVersion()
{
  t_ctorTlsVersion = SF_TLS_VERSION_UNSET;
}

void RegisterTlsHttpClientFactory()
{
  // SetHttpClientFactory() internally calls CleanupHttp() (tearing down the
  // default factory's curl global state) but does NOT call our InitStaticState.
  // The explicit InitHttp() afterward runs our InitStaticState -> re-inits curl.
  Aws::Http::SetHttpClientFactory(Aws::MakeShared<TlsCurlHttpClientFactory>(SF_TLS_HTTP_TAG));
  Aws::Http::InitHttp();
}

}
}
