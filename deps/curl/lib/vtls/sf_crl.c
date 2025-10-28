#include "curl_setup.h"
#include "urldata.h"
#include "sendf.h"
#include "sf_ocsp.h"
#include "sf_crl.h"

#include <stdbool.h>
#include <openssl/x509v3.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>


#ifdef _WIN32
#define strcasecmp _stricmp
#include <windows.h>
typedef HANDLE SF_MUTEX_HANDLE;
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#else
typedef pthread_mutex_t SF_MUTEX_HANDLE;
#endif

static SF_MUTEX_HANDLE crl_response_cache_mutex;

static int _mutex_init(SF_MUTEX_HANDLE *lock);
static int _mutex_lock(SF_MUTEX_HANDLE *lock);
static int _mutex_unlock(SF_MUTEX_HANDLE *lock);
static int _mutex_term(SF_MUTEX_HANDLE *lock);


/* Mutex */
int _mutex_init(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
  *lock = CreateMutex(
	NULL,  // default security attribute
	FALSE, // initially not owned
	NULL   // unnamed mutext
  );
  return 0;
#else
  return pthread_mutex_init(lock, NULL);
#endif
}

int _mutex_lock(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
  DWORD ret = WaitForSingleObject(*lock, INFINITE);
  return ret == WAIT_OBJECT_0 ? 0 : 1;
#else
  return pthread_mutex_lock(lock);
#endif
}

int _mutex_unlock(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
  return ReleaseMutex(*lock) == 0;
#else
  return pthread_mutex_unlock(lock);
#endif
}

int _mutex_term(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
  return CloseHandle(*lock) == 0;
#else
  return pthread_mutex_destroy(lock);
#endif
}

/************************************************************************************
 * Helpers to map X509_STORE* to configuration parameters 
 ************************************************************************************/
struct store_ctx_entry {
  const X509_STORE *ctx;
  struct Curl_easy *data;
  int crl_advisory;
  int crl_allow_no_crl;
  int crl_disk_caching;
  int crl_memory_caching;
  int curr_crl_num;
  long crl_download_timeout;  /* Timeout in seconds for CRL download */
};

struct store_ctx_array {
  struct store_ctx_entry   *entries;
  size_t                    size;     /* number of active entries */
  size_t                    capacity; /* total allocated slots */
};

static struct store_ctx_array sctx_registry = {NULL, 0, 0};

static int sctx_ensure_capacity()
{
  size_t new_capacity;
  struct store_ctx_entry *new_entries;

  if (sctx_registry.size >= sctx_registry.capacity) {
    new_capacity = sctx_registry.capacity + 8;
    new_entries = OPENSSL_realloc(sctx_registry.entries, new_capacity * sizeof(struct store_ctx_entry));
    if (!new_entries)
      return 0;
    sctx_registry.entries = new_entries;
    sctx_registry.capacity = new_capacity;
  }
  return 1;
}

static void sctx_register(const X509_STORE *ctx, struct Curl_easy *data, bool crl_advisory,
                          bool crl_allow_no_crl, bool crl_disk_caching, bool crl_memory_caching,
                          long crl_download_timeout)
{
  if (!sctx_ensure_capacity())
    return;
  sctx_registry.entries[sctx_registry.size].ctx = ctx;
  sctx_registry.entries[sctx_registry.size].data = data;
  sctx_registry.entries[sctx_registry.size].crl_advisory = crl_advisory;
  sctx_registry.entries[sctx_registry.size].crl_allow_no_crl = crl_allow_no_crl;
  sctx_registry.entries[sctx_registry.size].crl_disk_caching = crl_disk_caching;
  sctx_registry.entries[sctx_registry.size].crl_memory_caching = crl_memory_caching;
  sctx_registry.entries[sctx_registry.size].curr_crl_num = -1;
  sctx_registry.entries[sctx_registry.size].crl_download_timeout = crl_download_timeout;
  sctx_registry.size++;
}

static struct store_ctx_entry *sctx_lookup(const X509_STORE *ctx)
{
  for (size_t i = 0; i < sctx_registry.size; ++i) {
    if (sctx_registry.entries[i].ctx == ctx)
      return &sctx_registry.entries[i];
  }
  return NULL;
}

static void sctx_clear()
{
  OPENSSL_free(sctx_registry.entries);
  sctx_registry.entries = NULL;
  sctx_registry.capacity = 0;
  sctx_registry.size = 0;
}

static void sctx_unregister(const X509_STORE *ctx)
{
  if (!sctx_registry.entries || !ctx) {
    return;
  }

  for (size_t i = 0; i < sctx_registry.size; ++i) {
    if (sctx_registry.entries[i].ctx == ctx) {
      /* Move last entry to freed spot for fast removal */
      sctx_registry.entries[i] = sctx_registry.entries[sctx_registry.size - 1];
      sctx_registry.size--;

      if (sctx_registry.size == 0) {
        sctx_clear();
      }
      return;
    }
  }
}

static bool is_crl_newer(const X509_CRL *crl_old, const X509_CRL *crl_new)
{
  if (!crl_new)
    return false;
  if (!crl_old)
    return true;

  return ASN1_TIME_compare(X509_CRL_get0_lastUpdate(crl_new), X509_CRL_get0_lastUpdate(crl_old)) == 1;
}

static bool is_crl_expired(const X509_CRL *crl) {
  if (!crl) {
    return false;
  }

  ASN1_TIME *now = ASN1_TIME_set(NULL, time(NULL));
  const int cmp = ASN1_TIME_compare(X509_CRL_get0_nextUpdate(crl), now);
  ASN1_TIME_free(now);

  return cmp <= 0;
}

/************************************************************************************
 * Helpers for CRL memory cache, map uri to X509_CRL*
 ************************************************************************************/
struct uri_crl_entry {
  char *uri;
  X509_CRL *crl;
};

struct uri_crl_array {
  struct uri_crl_entry  *entries;
  size_t                 size;     /* number of active entries */
  size_t                 capacity; /* total allocated slots */
};

static struct uri_crl_array ucrl_registry = {NULL, 0, 0};

static int ucrl_ensure_capacity()
{
  size_t new_capacity;
  struct uri_crl_entry *new_entries;

  if (ucrl_registry.size >= ucrl_registry.capacity) {
    new_capacity = ucrl_registry.capacity + 8;
    new_entries = OPENSSL_realloc(ucrl_registry.entries, new_capacity * sizeof(struct uri_crl_entry));
    if (!new_entries)
      return 0;
    ucrl_registry.entries = new_entries;
    ucrl_registry.capacity = new_capacity;
  }
  return 1;
}

static void ucrl_register(const char *uri, const X509_CRL *crl)
{
  if (!ucrl_ensure_capacity())
    return;
  ucrl_registry.entries[ucrl_registry.size].uri = OPENSSL_malloc(PATH_MAX);
  if (ucrl_registry.entries[ucrl_registry.size].uri)
  {
    strncpy(ucrl_registry.entries[ucrl_registry.size].uri, uri, PATH_MAX);
    ucrl_registry.entries[ucrl_registry.size].crl = X509_CRL_dup(crl);
    ucrl_registry.size++;
  }
}

static struct uri_crl_entry *ucrl_lookup(const char *uri)
{
  for (size_t i = 0; i < ucrl_registry.size; ++i) {
    if (strcmp(ucrl_registry.entries[i].uri, uri) == 0)
      return &ucrl_registry.entries[i];
  }
  return NULL;
}

static void ucrl_clear()
{
  for (size_t i = 0; i < ucrl_registry.size; ++i) {
    /* Release objects */
    X509_CRL_free(ucrl_registry.entries[i].crl);
    OPENSSL_free(ucrl_registry.entries[i].uri);
  }
  OPENSSL_free(ucrl_registry.entries);
  ucrl_registry.entries = NULL;
  ucrl_registry.capacity = 0;
  ucrl_registry.size = 0;
}

static void ucrl_unregister(const char *uri)
{
  if (!ucrl_registry.entries || !uri) {
    return;
  }

  for (size_t i = 0; i < ucrl_registry.size; ++i) {
    if (ucrl_registry.entries[i].uri == uri) {

      /* Release objects */
      X509_CRL_free(ucrl_registry.entries[i].crl);
      OPENSSL_free(ucrl_registry.entries[i].uri);

      /* Move last entry to freed spot for fast removal */
      ucrl_registry.entries[i] = ucrl_registry.entries[ucrl_registry.size - 1];
      ucrl_registry.size--;

      if (ucrl_registry.size == 0) {
        ucrl_clear();
      }
      return;
    }
  }
}

/************************************************************************************
 * CRL caching
 ************************************************************************************/

static int is_valid_filename_char(char c)
{
  // Forbidden on Linux, Windows and Mac: < > : " / \ | ? *
  if (c == '<' || c == '>' || c == ':' || c == '"' ||
      c == '/' || c == '\\' || c == '|' || c == '?' || c == '*' || c == '\0') {
    return 0;
  }
  return 1;
}

static void normalize_filename(char* file_name)
{
  for (int i = strnlen(file_name, PATH_MAX) - 1; i >= 0; --i) {
    if (!is_valid_filename_char(file_name[i]))
      file_name[i] = '_';
  }
}

static const char *get_dp_url(DIST_POINT *dp)
{
  GENERAL_NAMES *gens;
  GENERAL_NAME *gen;
  int i, gtype;
  ASN1_STRING *uri;

  if (!dp->distpoint || dp->distpoint->type != 0)
    return NULL;
  gens = dp->distpoint->name.fullname;
  for (i = 0; i < sk_GENERAL_NAME_num(gens); i++) {
    gen = sk_GENERAL_NAME_value(gens, i);
    uri = GENERAL_NAME_get0_value(gen, &gtype);
    if (gtype == GEN_URI && ASN1_STRING_length(uri) > 6) {
      const char *uptr = (const char *)ASN1_STRING_get0_data(uri);
      return uptr;
    }
  }
  return NULL;
}

static const char* mkdir_if_not_exists(const struct Curl_easy *data, const char* dir)
{
#ifdef _WIN32
  int result = _mkdir(dir);
#else
  int result = mkdir(dir, 0700);
#endif
  if (result != 0 && errno != EEXIST)
  {
    failf(data, "Failed to create %s directory. Ignored. Error: %d",
          dir, errno);
    return NULL;
  }
  infof(data, "Created %s directory.", dir);
  return dir;
}

static char* ensure_cache_dir(const struct Curl_easy *data, char* cache_dir)
{
#ifdef __linux__
  char *home_env = getenv("HOME");
  if (home_env == NULL) {
    return NULL;
  }
  strcpy(cache_dir, home_env);

  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/.cache");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/snowflake");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/crls");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/");
#elif defined(__APPLE__)
  char *home_env = getenv("HOME");
  if (home_env == NULL) {
    return NULL;
  }
  strcpy(cache_dir, home_env);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/Library");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/Caches");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/Snowflake");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/crls");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strcat(cache_dir, "/");
#elif  defined(_WIN32)
  char *home_env = getenv("LOCALAPPDATA");
  if (home_env == NULL) {
    return NULL;
  }
  strncat(cache_dir, "\\Snowflake", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "\\Caches", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "\\crls", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "\\", PATH_MAX);
#endif
  return cache_dir;
}

static void get_cache_dir(const struct Curl_easy *data, char* cache_dir)
{
  const char *env_dir;

  cache_dir[0] = 0;

  env_dir = getenv("SF_CRL_RESPONSE_CACHE_DIR");
  infof(data, "CRL cache directory from environment: %s", env_dir ? env_dir : "(not set)");
  if (env_dir) {
    strcpy(cache_dir, env_dir);
#if defined(_WIN32)
    const size_t len = strnlen(cache_dir, PATH_MAX);
    if (cache_dir[len-1] != '\\') {
      strncat(cache_dir, "\\", PATH_MAX - len);
    }
#else
    const size_t len = strnlen(cache_dir, PATH_MAX);
    if (cache_dir[len-1] != '/') {
      strcat(cache_dir, "/");
    }
#endif
  }
  else {
    ensure_cache_dir(data, cache_dir);
  }
}

static void get_file_path_by_uri(const struct store_ctx_entry *data, const char *uri, char* file_path)
{
  get_cache_dir(data->data, file_path);
  if (*file_path) {
    char file_name[PATH_MAX] = {};
    strncpy(file_name, uri, PATH_MAX);
    normalize_filename(file_name);
    strncat(file_path, file_name, PATH_MAX);
  }
}

static void save_crl_in_memory(const struct store_ctx_entry *data, const char *uri,
                               X509_CRL **pcrl)
{
  if (!data->crl_memory_caching)
    return;

  ucrl_unregister(uri);
  ucrl_register(uri, *pcrl);
}

static void save_crl_to_disk(const struct store_ctx_entry *data, const char *uri,
                             X509_CRL **pcrl)
{
  BIO *fp;
  char file_path[PATH_MAX] = {};

  if (!data->crl_disk_caching) {
    infof(data->data, "CRL disk caching is disabled. Not saving CRL to disk. (URI: %s)", uri);
    return;
  }

  infof(data->data, "CRL disk caching is enabled. Saving CRL to disk: (URI: %s)", uri);

  if (*pcrl != NULL && data->crl_disk_caching) {
    get_file_path_by_uri(data, uri, file_path);
    if (*file_path) {
      fp = BIO_new_file(file_path, "w");
      if (fp) {
        if (!PEM_write_bio_X509_CRL(fp, *pcrl))
          infof(data->data, "Cannot save CRL content to file: %s", file_path);
        BIO_free(fp);
      }
      else {
        infof(data->data, "Cannot open CRL file to save (errno %d): %s", errno, file_path);
      }
    }
    else {
      infof(data->data, "Cannot resolve path for CRL cache");
    }
  }
}

static void get_crl_from_memory(const struct store_ctx_entry *data, const char *uri,
                                X509_CRL **pcrl)
{
  struct uri_crl_entry *ucrl;
  if (!data->crl_memory_caching)
    return;

  // lookup for CRL in memory cache
  ucrl = ucrl_lookup(uri);

  if (ucrl) {
    *pcrl = X509_CRL_dup(ucrl->crl);
  }
  if (*pcrl)
    infof(data->data, "CRL loaded from memory: %s", uri);

}

#ifdef _WIN32
#define sf_fstat _fstat
#define sf_stat _stat
#else
#define sf_fstat fstat
#define sf_stat stat
#endif

static void get_crl_from_disk(const struct store_ctx_entry *data, const char *uri,
                              X509_CRL **pcrl)
{
  BIO *fp;
  char file_path[PATH_MAX] = {};

  if (!data->crl_disk_caching) {
    infof(data->data, "CRL disk caching is disabled. Not loading CRL from disk (URI: %s)", uri);
    return;
  }

  infof(data->data, "CRL disk caching is enabled. Loading CRL from disk (URI: %s)", uri);

  // lookup for CRL on disk
  infof(data->data, "Tweak: %s %s", __DATE__, __TIME__);
  get_file_path_by_uri(data, uri, file_path);
  if (*file_path) {
    infof(data->data, "Reading the file");
    fp = BIO_new_file(file_path, "r");
    if (fp) {
      struct sf_stat file_stats;
      long fd = BIO_get_fd(fp, NULL);
      if (fd != -1) {
        if (sf_fstat(fd, &file_stats) == 0) {
          *pcrl = PEM_read_bio_X509_CRL(fp, NULL, NULL, NULL);
        }
        else {
          infof(data->data, "Cannot get file status: %s", file_path);
        }
      }
      else {
        infof(data->data, "Cannot obtain file descriptor: %s", file_path);
      }

      BIO_free(fp);
    }
    else {
      infof(data->data, "Cannot open file to read (errno %d): %s", errno, file_path);
    }
  }

  if (*pcrl)
    infof(data->data, "CRL loaded from disk: %s", uri);
}

static bool get_crl_from_cache(const struct store_ctx_entry *data, const char *uri,
                              X509_CRL **pcrl)
{
  *pcrl = NULL;

  _mutex_lock(&crl_response_cache_mutex);

  get_crl_from_memory(data, uri, pcrl);
  if (!*pcrl) {
    get_crl_from_disk(data, uri, pcrl);
    if (*pcrl) {
      save_crl_in_memory(data, uri, pcrl);
    }
  }

  _mutex_unlock(&crl_response_cache_mutex);

  if (!*pcrl)
    return false;

  if (is_crl_expired(*pcrl)) {
    infof(data->data, "CRL need to be updated: %s", uri);

    return false;
  }

  return true;
}

static void save_crl_to_cache(struct store_ctx_entry *data, const char *uri, X509_CRL *crl)
{
  _mutex_lock(&crl_response_cache_mutex);

  save_crl_to_disk(data, uri, &crl);
  save_crl_in_memory(data, uri, &crl);

  _mutex_unlock(&crl_response_cache_mutex);
}

static X509_CRL *load_crl(struct store_ctx_entry *data, const char *uri)
{
  X509_CRL *crl_cached = NULL;
  X509_CRL *crl = NULL;

  if (get_crl_from_cache(data, uri, &crl_cached))
    return crl_cached;

  BIO *mem = OSSL_HTTP_get(uri, NULL /* proxy */, NULL /* no_proxy */,
                           NULL, NULL, NULL /* cb */, NULL /* arg */,
                           1024 /* buf_size */, NULL /* headers */,
                           NULL /* expected_ct */, 1 /* expect_asn1 */,
                           0, data->crl_download_timeout);

  ASN1_VALUE *res = ASN1_item_d2i_bio(ASN1_ITEM_rptr(X509_CRL), mem, NULL);

  BIO_free(mem);
  crl = (X509_CRL *)res;

  if (crl)
    infof(data->data, "CRL loaded from http: %s", uri);
  else
    infof(data->data, "CRL cannot be loaded from http: %s", uri);

  if (crl &&
      (!crl_cached ||
        is_crl_newer(crl_cached, crl))) {
    save_crl_to_cache(data, uri, crl);
    return crl;
  }

  /* don't need crl anymore */
  X509_CRL_free(crl);

  if (crl_cached
      && !is_crl_expired(crl_cached)) {
    return crl_cached;
  }

  return NULL;
}

static void load_crls_crldp(struct store_ctx_entry *data,
                            STACK_OF(X509_CRL) *crls,
                            STACK_OF(DIST_POINT) *crldp)
{
  int i;
  const char *urlptr = NULL;
  X509_CRL *crl = NULL;

  for (i = 0; i < sk_DIST_POINT_num(crldp); i++) {
    DIST_POINT *dp = sk_DIST_POINT_value(crldp, i);
    urlptr = get_dp_url(dp);
    if (urlptr != NULL) {
      crl = load_crl(data, urlptr);
      sk_X509_CRL_push(crls, crl);
    }
  }
}

/************************************************************************************
 * CRL validation handlers
 ************************************************************************************/
static STACK_OF(X509_CRL) *lookup_crls_handler(const X509_STORE_CTX *ctx,
                                               const X509_NAME *nm)
{
  X509 *x;
  STACK_OF(X509_CRL) *crls = NULL;
  STACK_OF(DIST_POINT) *crldp;
  struct store_ctx_entry *data;

  data = sctx_lookup(X509_STORE_CTX_get0_store(ctx));

  if (!data)
    return NULL;

  data->curr_crl_num = 0;

  const char *no_crl = getenv("SF_TEST_CRL_NO_CRL");
  if (no_crl && strcasecmp(no_crl, "true") == 0) {
    infof(data->data, "SF_TEST_CRL_NO_CRL is set, no CRL will be loaded");
    return NULL;
  }

  crls = sk_X509_CRL_new_null();

  if (!crls)
    return NULL;

  x = X509_STORE_CTX_get_current_cert(ctx);
  crldp = X509_get_ext_d2i(x, NID_crl_distribution_points, NULL, NULL);
  load_crls_crldp(data, crls, crldp);
  sk_DIST_POINT_pop_free(crldp, DIST_POINT_free);

  if (sk_X509_CRL_num(crls) == 0) {
    sk_X509_CRL_free(crls);
    return NULL;
  }

  data->curr_crl_num = sk_X509_CRL_num(crls);
  return crls;
}

static int error_handler(int ok, X509_STORE_CTX *ctx)
{
  int err;
  X509 *err_cert;
  struct store_ctx_entry *data;
#define MAX_CERT_NAME_LEN 64
  char X509_cert_name[MAX_CERT_NAME_LEN + 1];

  if (ok)
    return ok;

  data = sctx_lookup(X509_STORE_CTX_get0_store(ctx));
  if (!data)
    return 0;

  err = X509_STORE_CTX_get_error(ctx);
  err_cert = X509_STORE_CTX_get_current_cert(ctx);

  if (err == X509_V_ERR_UNABLE_TO_GET_CRL) {
    if (X509_self_signed(err_cert, 1)) {
      infof(data->data, "CRL validation skipped error %d - self signed, subject=%s",
            err,
            X509_NAME_oneline(X509_get_subject_name(err_cert), X509_cert_name,
                              MAX_CERT_NAME_LEN));
      X509_STORE_CTX_set_error(ctx, 0);
      return 1;
    }
    if (data->crl_allow_no_crl && data->curr_crl_num == 0) {
      infof(data->data, "CRL validation skipped error %d - no crl in the certificate, subject=%s",
            err,
            X509_NAME_oneline(X509_get_subject_name(err_cert), X509_cert_name,
                              MAX_CERT_NAME_LEN));
      X509_STORE_CTX_set_error(ctx, 0);
      return 1;
    }
    if (data->crl_advisory) {
      infof(data->data, "CRL validation skipped error %d - advisory mode, subject=%s",
            err,
            X509_NAME_oneline(X509_get_subject_name(err_cert), X509_cert_name,
                              MAX_CERT_NAME_LEN));
      X509_STORE_CTX_set_error(ctx, 0);
      return 1;
    }
  }

  infof(data->data,
        "Certificate validation error, subject=%s, error %d at depth %d, %s",
        X509_NAME_oneline(X509_get_subject_name(err_cert), X509_cert_name,
                          MAX_CERT_NAME_LEN),
        err,
        X509_STORE_CTX_get_error_depth(ctx),
        X509_verify_cert_error_string(err));


  return 0;
}

static void term_crl()
{
  /* terminate the mutex */
  _mutex_term(&crl_response_cache_mutex);

  /* clear caches */
  sctx_clear();
  ucrl_clear();
}

SF_PUBLIC(void) registerCRLCheck(struct Curl_easy *data,
                                 X509_STORE *ctx,
                                 bool crl_advisory,
                                 bool crl_allow_no_crl,
                                 bool crl_disk_caching,
                                 bool crl_memory_caching,
                                 long crl_download_timeout)
{
  char cache_dir[PATH_MAX] = "";
  infof(data, "Registering SF CRL Validation...");
  infof(data, "crl_disk_caching: %s",
        crl_disk_caching ? "enabled" : "disabled");

  get_cache_dir(data, cache_dir);
  if (*cache_dir)
    infof(data, "CRL cache file directory: %s", cache_dir);
  else
    infof(data, "CRL cache file directory not exists!");


  /* register handler to read CRLs */
  X509_STORE_set_lookup_crls_cb(ctx, lookup_crls_handler);

  /* register handler to skip CRL errors when needed */
  X509_STORE_set_verify_cb(ctx, error_handler);

  X509_STORE_set_flags(ctx, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL | X509_V_FLAG_X509_STRICT);

  /* register X509_STORE with given parameters */
  sctx_register(ctx, data,
               crl_advisory, crl_allow_no_crl, crl_disk_caching, crl_memory_caching,
               crl_download_timeout);
}

SF_PUBLIC(void) initCertCRL()
{
  /* must call only once. not thread safe */
  _mutex_init(&crl_response_cache_mutex);
  atexit(term_crl);
}
