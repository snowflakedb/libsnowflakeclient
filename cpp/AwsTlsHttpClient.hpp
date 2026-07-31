#ifndef SNOWFLAKE_AWS_TLS_HTTP_CLIENT_HPP
#define SNOWFLAKE_AWS_TLS_HTTP_CLIENT_HPP

/*
 * Per-client TLS version override for the AWS SDK for C++
 *
 * AWS SDK 1.11.806 offers no per-client TLS-version field, and its
 * ClientConfiguration is sliced to the base type before reaching the HTTP
 * client factory (AWSClient ctor: `ClientConfiguration tempConfig(...)`), so a
 * derived-config + dynamic_cast channel does NOT work. Instead we:
 *   - subclass CurlHttpClient and override the SDK's designated per-request
 *     extension point OverrideOptionsOnConnectionHandle() to apply
 *     CURLOPT_SSLVERSION (via the shared sf_apply_tls_version helper);
 *   - register a custom HttpClientFactory that stamps each created client with
 *     a tls_version read from a thread-local that is set only for the duration
 *     of the synchronous `new S3Client(...)` construction (ScopedAwsTlsVersion).
 *
 * The thread-local is construction-scoped (not request-scoped): the HTTP client
 * is created synchronously, once, on the constructing thread inside the S3Client
 * constructor, so there is no pool/async race.
 */

#include <aws/core/http/curl/CurlHttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/client/ClientConfiguration.h>
#include <memory>

namespace Snowflake::Client
{
    class TlsCurlHttpClient : public Aws::Http::CurlHttpClient
    {
    public:
        TlsCurlHttpClient(const Aws::Client::ClientConfiguration& clientConfig, long tlsVersion);

        // Test purpose
        long getConfiguredTlsVersion() const { return m_tlsVersion; }

    protected:
        // Exact SDK signature. Called after the SDK's own CURLOPT_SSLVERSION and
        // just before curl_easy_perform, so our value wins. Touches SSLVERSION only.
        void OverrideOptionsOnConnectionHandle(CURL* connectionHandle) const override;

    private:
        const long m_tlsVersion;
    };

    /** Factory that mirrors DefaultHttpClientFactory but returns TlsCurlHttpClient. */
    class TlsCurlHttpClientFactory : public Aws::Http::HttpClientFactory
    {
    public:
        std::shared_ptr<Aws::Http::HttpClient>
            CreateHttpClient(const Aws::Client::ClientConfiguration& clientConfiguration) const override;

        std::shared_ptr<Aws::Http::HttpRequest>
            CreateHttpRequest(const Aws::String& uri, Aws::Http::HttpMethod method,
                const Aws::IOStreamFactory& streamFactory) const override;

        std::shared_ptr<Aws::Http::HttpRequest>
            CreateHttpRequest(const Aws::Http::URI& uri, Aws::Http::HttpMethod method,
                const Aws::IOStreamFactory& streamFactory) const override;

        void InitStaticState() override;
        void CleanupStaticState() override;
    };

    /**
     * RAII guard bridging the per-client tls_version into the factory across the
     * synchronous S3Client construction. Set the value, construct the client, and
     * the guard restores "unset" on scope exit.
     */
    class ScopedAwsTlsVersion
    {
    public:
        explicit ScopedAwsTlsVersion(long tlsVersion);
        ~ScopedAwsTlsVersion();
        ScopedAwsTlsVersion(const ScopedAwsTlsVersion&) = delete;
        ScopedAwsTlsVersion& operator=(const ScopedAwsTlsVersion&) = delete;
    };

    /**
     * Install TlsCurlHttpClientFactory as the global AWS HTTP client factory.
     * Call exactly once, immediately after Aws::InitAPI().
     */
    void RegisterTlsHttpClientFactory();

}

#endif // SNOWFLAKE_AWS_TLS_HTTP_CLIENT_HPP
