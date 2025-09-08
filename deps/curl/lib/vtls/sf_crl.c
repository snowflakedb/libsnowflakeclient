#include "curl_setup.h"
#include "urldata.h"
#include "sendf.h"
#include "sf_ocsp.h"
#include "sf_crl.h"

#include <openssl/x509v3.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

// Connection timeout in seconds for CRL download
#define CRL_DOWNLOAD_TIMEOUT 30L

#ifdef _WIN32
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
                          bool crl_allow_no_crl, bool crl_disk_caching, bool crl_memory_caching)
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

/************************************************************************************
 * Helpers for CRL memory cache, map uri to X509_CRL*
 ************************************************************************************/
struct uri_crl_entry {
  char *uri;
  X509_CRL *crl;
  time_t download_time;
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

static void ucrl_register(const char *uri, const X509_CRL *crl, time_t download_time)
{
  if (!ucrl_ensure_capacity())
    return;
  ucrl_registry.entries[ucrl_registry.size].uri = OPENSSL_malloc(strlen(uri) + 1);
  if (ucrl_registry.entries[ucrl_registry.size].uri)
  {
    strcpy(ucrl_registry.entries[ucrl_registry.size].uri, uri);
    ucrl_registry.entries[ucrl_registry.size].crl = X509_CRL_dup(crl);
    ucrl_registry.entries[ucrl_registry.size].download_time = download_time;
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
 * CLR caching
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
  for (int i = strlen(file_name) - 1; i >= 0; --i) {
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

static char* mkdir_if_not_exists(const struct Curl_easy *data, char* dir)
{
#ifdef _WIN32
  int result = _mkdir(dir);
#else
  int result = mkdir(dir, 0755);
#endif
  if (result != 0)
  {
    if (errno != EEXIST)
    {
      failf(data, "Failed to create %s directory. Ignored. Error: %d",
            dir, errno);
      return NULL;

    }
  }
  else
  {
    infof(data, "Created %s directory.", dir);
  }
  return dir;
}

static char* ensure_cache_dir(const struct Curl_easy *data, char* cache_dir)
{
#ifdef __linux__
  char *home_env = getenv("HOME");
  strcpy(cache_dir, (home_env == NULL ? (char*)"/tmp" : home_env));

  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/.cache");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/snowflake");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/crls");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/");
#elif defined(__APPLE__)
  char *home_env = getenv("HOME");
  strcpy(cache_dir, (home_env == NULL ? (char*)"/tmp" : home_env));
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/Library");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/Caches");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/Snowflake");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/crls");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/");
#elif  defined(_WIN32)
  char *home_env = getenv("USERPROFILE");
  if (home_env == NULL)
  {
    home_env = getenv("TMP");
	if (home_env == NULL)
    {
      home_env = getenv("TEMP");
    }
  }
  strcpy(cache_dir, (home_env == NULL ? (char*)"c:\\temp" : home_env));
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\AppData");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\Local");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\Snowflake");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\Caches");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\crls");
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\");
#endif
  return cache_dir;
err:
  return NULL;
}

static void get_cache_dir(const struct Curl_easy *data, char* cache_dir)
{
  const char *env_dir;

  cache_dir[0] = 0;

  env_dir = getenv("SF_CRL_RESPONSE_CACHE_DIR");
  if (env_dir) {
    strcpy(cache_dir, env_dir);
  }
  else {
    ensure_cache_dir(data, cache_dir);
  }
}

static void get_file_path_by_uri(const struct store_ctx_entry *data, const char *uri, char* file_path)
{
  get_cache_dir(data->data, file_path);

  if (*file_path) {
    char *file_name = file_path + strlen(file_path);
    strcpy(file_name, uri);
    normalize_filename(file_name);
  }
}

static void save_crl_in_memory(const struct store_ctx_entry *data, const char *uri,
                               X509_CRL **pcrl, time_t *last_modified)
{
  if (!data->crl_memory_caching)
    return;

  ucrl_unregister(uri);
  ucrl_register(uri, *pcrl, *last_modified);
}

static void save_crl_to_disk(const struct store_ctx_entry *data, const char *uri,
                             X509_CRL **pcrl)
{
  BIO *fp;
  char file_path[PATH_MAX];

  if (!data->crl_disk_caching)
    return;

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
                                X509_CRL **pcrl, time_t *download_time)
{
  struct uri_crl_entry *ucrl;
  if (!data->crl_memory_caching)
    return;

  // lookup for CRL in memory cache
  ucrl = ucrl_lookup(uri);

  if (ucrl) {
    *download_time = ucrl->download_time;
    *pcrl = X509_CRL_dup(ucrl->crl);
  }
  if (*pcrl)
    infof(data->data, "CRL loaded from memory: %s", uri);
}

static void get_crl_from_disk(const struct store_ctx_entry *data, const char *uri,
                              X509_CRL **pcrl, time_t *download_time)
{
  BIO *fp;
  char file_path[PATH_MAX];

  if (!data->crl_disk_caching)
    return;

  // lookup for CRL on disk
  get_file_path_by_uri(data, uri, file_path);
  if (*file_path) {
    fp = BIO_new_file(file_path, "r");
    if (fp) {
      struct stat file_stats;
      long fd = BIO_get_fd(fp, NULL);
      if (fd != -1) {
        if (fstat(fd, &file_stats) == 0) {
          *download_time = file_stats.st_mtime;
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

static int get_crl_from_cache(const struct store_ctx_entry *data, const char *uri,
                              X509_CRL **pcrl)
{
  int day, sec;
  time_t download_time;
  char* validity_days_str;
  int validity_days = 0;
  time_t validity_time;

  *pcrl = NULL;

  _mutex_lock(&crl_response_cache_mutex);

  get_crl_from_memory(data, uri, pcrl, &download_time);
  if (!*pcrl) {
    get_crl_from_disk(data, uri, pcrl, &download_time);
    if (*pcrl) {
      save_crl_in_memory(data, uri, pcrl, &download_time);
    }
  }

  _mutex_unlock(&crl_response_cache_mutex);

  if (!*pcrl)
    return 0;

  validity_days_str = getenv("SF_CRL_VALIDITY_TIME");
  if (validity_days_str)
    validity_days = atoi(validity_days_str);
  if (validity_days == 0)
    validity_days = 10; /* default 10 days */

  validity_time = download_time + (validity_days * 86400);
  if (validity_time < time(NULL)) {
    infof(data->data, "CRL validity time (%d days) expired, need reloading: %s", validity_days, uri);
    return 0;
  }

  if (ASN1_TIME_diff(&day, &sec, NULL, X509_CRL_get0_nextUpdate(*pcrl))
      && day <= 0 && sec <= 0) {
    infof(data->data, "CRL need to be updated: %s", uri);
    return 0;
  }
  return 1;
}

static void save_crl_to_cache(struct store_ctx_entry *data, const char *uri, X509_CRL *crl)
{
  time_t curr_time = time(NULL);

  _mutex_lock(&crl_response_cache_mutex);

  save_crl_to_disk(data, uri, &crl);
  save_crl_in_memory(data, uri, &crl, &curr_time);

  _mutex_unlock(&crl_response_cache_mutex);
}

static X509_CRL *load_crl(struct store_ctx_entry *data, const char *uri)
{
  int day, sec;
  X509_CRL *crl_cached = NULL;
  X509_CRL *crl = NULL;

  if (get_crl_from_cache(data, uri, &crl_cached))
    return crl_cached;

  BIO *mem = OSSL_HTTP_get(uri, NULL /* proxy */, NULL /* no_proxy */,
                           NULL, NULL, NULL /* cb */, NULL /* arg */,
                           1024 /* buf_size */, NULL /* headers */,
                           NULL /* expected_ct */, 1 /* expect_asn1 */,
                           0, CRL_DOWNLOAD_TIMEOUT);

  ASN1_VALUE *res = ASN1_item_d2i_bio(ASN1_ITEM_rptr(X509_CRL), mem, NULL);

  BIO_free(mem);
  crl = (X509_CRL *)res;

  if (crl)
    infof(data->data, "CRL loaded from http: %s", uri);
  else
    infof(data->data, "CRL cannot be loaded from http: %s", uri);

  if (crl &&
      (!crl_cached ||
         (ASN1_TIME_diff(&day, &sec,
                         X509_CRL_get0_lastUpdate(crl_cached),
                         X509_CRL_get0_lastUpdate(crl))
          && day >= 0 && sec >= 0))) {
    save_crl_to_cache(data, uri, crl);
  }
  else
  {
    /* don't need crl anymore */
    X509_CRL_free(crl);

    if (crl_cached
        && ASN1_TIME_diff(&day, &sec,
                          NULL,
                          X509_CRL_get0_nextUpdate(crl_cached))
        && day >= 0 && sec >= 0) {
      crl = crl_cached;
    }
    else {
      crl = NULL;
    }
  }
  return crl;
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
 * CLR validation handlers
 ************************************************************************************/
static STACK_OF(X509_CRL) *lookup_crls_handler(const X509_STORE_CTX *ctx,
                                               const X509_NAME *nm)
{
  X509 *x;
  STACK_OF(X509_CRL) *crls = NULL;
  STACK_OF(DIST_POINT) *crldp;
  struct store_ctx_entry *data;

  if (getenv("SF_TEST_CRL_NO_CRL"))
    return NULL;

  data = sctx_lookup(X509_STORE_CTX_get0_store(ctx));

  if (!data)
    return NULL;

  data->curr_crl_num = 0;
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
#define MAX_CERT_NAME_LEN 100
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
                                 bool crl_memory_caching)
{
  char cache_dir[PATH_MAX] = "";
  infof(data, "Registering SF CRL Validation...");
  //return;
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
               crl_advisory, crl_allow_no_crl, crl_disk_caching, crl_memory_caching);
}

SF_PUBLIC(void) initCertCRL()
{
  /* must call only once. not thread safe */
  _mutex_init(&crl_response_cache_mutex);
  atexit(term_crl);
}