#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <curl/curl.h>
extern CURLcode encodeUrlData(const char *url_data, size_t data_size, char** outptr, size_t *outlen);

static void assert_eq(const char *label, const char *got, const char *want) {
  if (strcmp(got, want) != 0) {
    fprintf(stderr, "%s FAILED: got='%s' want='%s'\n", label, got, want);
    exit(1);
  }
}

int main(void) {
  char *enc = NULL;

  /* Space and reserved chars */
  encodeUrlData("a b?c&d=e", strlen("a b?c&d=e"), &enc, NULL);
  assert_eq("reserved", enc, "a%20b%3Fc%26d%3De");
  free(enc);

  /* Slash and tilde */
  encodeUrlData("/path/with~tilde", strlen("/path/with~tilde"), &enc, NULL);
  assert_eq("slash-tilde", enc, "%2Fpath%2Fwith~tilde");
  free(enc);

  /* All unreserved stay literal */
  encodeUrlData("Az09-_.~", strlen("Az09-_.~"), &enc, NULL);
  assert_eq("unreserved", enc, "Az09-_.~");
  free(enc);

  fprintf(stderr, "OK\n");
  return 0;
}


