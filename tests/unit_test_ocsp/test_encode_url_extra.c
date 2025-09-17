#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <curl/curl.h>
extern CURLcode encodeUrlData(const char *url_data, size_t data_size, char** outptr, size_t *outlen);

static void dump_case(const char *case_name, const char *src, size_t src_len, CURLcode rc, const char *enc, size_t outlen) {
  fprintf(stderr, "[%s] src_len=%zu rc=%d enc=%p outlen=%zu", case_name, src_len, (int)rc, (void*)enc, outlen);
  if (enc) {
    size_t sl = strlen(enc);
    fprintf(stderr, " strlen(enc)=%zu enc='%s'\n", sl, enc);
  } else {
    fprintf(stderr, " enc=NULL\n");
  }
  fflush(stderr);
}

static void assert_eq(const char *label, const char *got, const char *want) {
  if (strcmp(got, want) != 0) {
    fprintf(stderr, "%s FAILED: got='%s' want='%s'\n", label, got, want);
    exit(1);
  }
}

int main(void) {
  /* Make stderr unbuffered so CI logs show incremental steps */
  setvbuf(stderr, NULL, _IONBF, 0);

  char *enc = NULL;
  size_t outlen = 0;
  CURLcode rc;

  /* Space and reserved chars */
  fprintf(stderr, "[reserved] BEFORE call\n");
  rc = encodeUrlData("a b?c&d=e", strlen("a b?c&d=e"), &enc, &outlen);
  dump_case("reserved", "a b?c&d=e", strlen("a b?c&d=e"), rc, enc, outlen);
  if (rc != CURLE_OK || !enc) {
    fprintf(stderr, "[reserved] ERROR rc=%d enc=%p\n", (int)rc, (void*)enc);
    return 2;
  }
  assert_eq("reserved", enc, "a%20b%3Fc%26d%3De");
  if (outlen != strlen(enc)) {
    fprintf(stderr, "[reserved] length mismatch outlen=%zu strlen(enc)=%zu\n", outlen, strlen(enc));
    return 3;
  }
  curl_free(enc);
  enc = NULL; outlen = 0;

  /* Slash and tilde */
  fprintf(stderr, "[slash-tilde] BEFORE call\n");
  rc = encodeUrlData("/path/with~tilde", strlen("/path/with~tilde"), &enc, &outlen);
  dump_case("slash-tilde", "/path/with~tilde", strlen("/path/with~tilde"), rc, enc, outlen);
  if (rc != CURLE_OK || !enc) {
    fprintf(stderr, "[slash-tilde] ERROR rc=%d enc=%p\n", (int)rc, (void*)enc);
    return 2;
  }
  assert_eq("slash-tilde", enc, "%2Fpath%2Fwith~tilde");
  if (outlen != strlen(enc)) {
    fprintf(stderr, "[slash-tilde] length mismatch outlen=%zu strlen(enc)=%zu\n", outlen, strlen(enc));
    return 3;
  }
  curl_free(enc);
  enc = NULL; outlen = 0;

  /* All unreserved stay literal */
  fprintf(stderr, "[unreserved] BEFORE call\n");
  rc = encodeUrlData("Az09-_.~", strlen("Az09-_.~"), &enc, &outlen);
  dump_case("unreserved", "Az09-_.~", strlen("Az09-_.~"), rc, enc, outlen);
  if (rc != CURLE_OK || !enc) {
    fprintf(stderr, "[unreserved] ERROR rc=%d enc=%p\n", (int)rc, (void*)enc);
    return 2;
  }
  assert_eq("unreserved", enc, "Az09-_.~");
  if (outlen != strlen(enc)) {
    fprintf(stderr, "[unreserved] length mismatch outlen=%zu strlen(enc)=%zu\n", outlen, strlen(enc));
    return 3;
  }
  curl_free(enc);
  enc = NULL; outlen = 0;

  fprintf(stderr, "OK\n");
  return 0;
}


