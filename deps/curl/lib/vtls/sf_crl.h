#ifndef HEADER_CURL_SF_CRL_H
#define HEADER_CURL_SF_CRL_H

SF_PUBLIC(void) registerCRLCheck(struct Curl_easy *data,
                                 X509_STORE *ctx,
                                 bool crl_advisory,
                                 bool crl_allow_no_crl,
                                 bool crl_disk_caching,
                                 bool crl_memory_caching,
                                 long crl_download_timeout);

SF_PUBLIC(void) initCertCRL();

#endif

