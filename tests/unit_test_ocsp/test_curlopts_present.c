#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

static void die(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

int main(void) {
  /* Sanity: the Snowflake-specific options must exist and be settable. */
  if ((int)CURLOPT_SSL_SF_OCSP_CHECK <= 0) die("CURLOPT_SSL_SF_OCSP_CHECK undefined");
  if ((int)CURLOPT_SSL_SF_OCSP_FAIL_OPEN <= 0) die("CURLOPT_SSL_SF_OCSP_FAIL_OPEN undefined");

  CURL *ch = NULL;
  if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) die("curl_global_init failed");
  ch = curl_easy_init();
  if (!ch) die("curl_easy_init failed");

  if (curl_easy_setopt(ch, CURLOPT_SSL_SF_OCSP_CHECK, 1L) != CURLE_OK)
    die("setopt OCSP_CHECK failed");
  if (curl_easy_setopt(ch, CURLOPT_SSL_SF_OCSP_FAIL_OPEN, 0L) != CURLE_OK)
    die("setopt OCSP_FAIL_OPEN failed");

  curl_easy_cleanup(ch);
  curl_global_cleanup();
  fprintf(stderr, "OK\n");
  return 0;
}


