#ifndef HEADER_CURL_SF_OCSP_H
#define HEADER_CURL_SF_OCSP_H

#include "urldata.h"
#include <openssl/ssl.h>
#include <openssl/ocsp.h>

#ifdef _WIN32
#define SF_PUBLIC(type)   __declspec(dllexport) type __stdcall
#else
#define SF_PUBLIC(type) type
#endif

SF_PUBLIC(CURLcode) initCertOCSP();
SF_PUBLIC(CURLcode) checkCertOCSP(struct connectdata *conn,
                                  struct Curl_easy *data,
                                  STACK_OF(X509) *ch,
                                  X509_STORE *st,
                                  int ocsp_failopen,
                                  bool oob_enable);

/* Test hooks are provided by unit tests; not exported from curl */

#endif

