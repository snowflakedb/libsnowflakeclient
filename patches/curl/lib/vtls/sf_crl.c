#include "curl_setup.h"
#include "curl_trc.h"
#include "urldata.h"
#include "sendf.h"
#include "sf_ocsp.h"
#include "sf_crl.h"

#include <stdbool.h>
#include <openssl/x509v3.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>


#ifdef _WIN32
#define strcasecmp _stricmp
#include <windows.h>
typedef HANDLE SF_MUTEX_HANDLE;
typedef HANDLE SF_THREAD_HANDLE;
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#else
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
typedef pthread_mutex_t SF_MUTEX_HANDLE;
typedef pthread_t SF_THREAD_HANDLE;
#endif

/* Created once and never destroyed, so a lock taken after term_crl() is valid. */
static SF_MUTEX_HANDLE crl_response_cache_mutex;
static int crl_response_cache_mutex_created = 0;
static SF_THREAD_HANDLE crl_cleanup_thread;
static volatile int crl_cleanup_stop = 0;
/* The flags below are only accessed under crl_response_cache_mutex. */
static int crl_cleanup_thread_started = 0;
static int crl_cleanup_thread_attempted = 0;
static int crl_cache_initialized = 0;
#ifndef _WIN32
/* Process that tried to start the cleanup thread. A forked child inherits the
 * flags above, but not the thread. */
static pid_t crl_cleanup_thread_pid = 0;
static int crl_atfork_error = 0;
#endif

static int _mutex_init(SF_MUTEX_HANDLE *lock);
static int _mutex_lock(SF_MUTEX_HANDLE *lock);
static int _mutex_unlock(SF_MUTEX_HANDLE *lock);


/* Mutex */
int _mutex_init(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
  *lock = CreateMutex(
	NULL,  // default security attribute
	FALSE, // initially not owned
	NULL   // unnamed mutext
  );
 if (*lock == NULL) {
       return 1;
 }
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

#ifdef _WIN32
static DWORD WINAPI crl_cleanup_thread_win(LPVOID arg);
#endif
static void *crl_cleanup_thread_proc(void *arg);

#ifdef _WIN32
static int _thread_init(SF_THREAD_HANDLE *thread, LPTHREAD_START_ROUTINE proc, void *arg)
{
  *thread = CreateThread(NULL, 0, proc, arg, 0, NULL);
  return *thread == NULL;
}
#else
static int _thread_init(SF_THREAD_HANDLE *thread, void *(*proc)(void *), void *arg)
{
  /* The thread inherits the caller's signal mask. Block everything, so signals
   * sent to the process are never handled on it, except faults: a blocked
   * fault kills the process without running its crash handlers. */
  sigset_t all, old;
  int ret;

  sigfillset(&all);
  sigdelset(&all, SIGSEGV);
  sigdelset(&all, SIGBUS);
  sigdelset(&all, SIGFPE);
  sigdelset(&all, SIGILL);
  pthread_sigmask(SIG_SETMASK, &all, &old);
  ret = pthread_create(thread, NULL, proc, arg);
  pthread_sigmask(SIG_SETMASK, &old, NULL);
  return ret;
}
#endif

static int _thread_join(SF_THREAD_HANDLE thread)
{
#ifdef _WIN32
  DWORD ret = WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
  return ret == WAIT_OBJECT_0 ? 0 : 1;
#else
  return pthread_join(thread, NULL);
#endif
}

static void _sleep_sec(void)
{
#ifdef _WIN32
  Sleep(1000);
#else
  sleep(1);
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
  long crl_download_max_size; /* Max CRL download size in bytes */
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

static struct store_ctx_entry *sctx_lookup(const X509_STORE *ctx)
{
  for (size_t i = 0; i < sctx_registry.size; ++i) {
    if (sctx_registry.entries[i].ctx == ctx)
      return &sctx_registry.entries[i];
  }
  return NULL;
}

static void sctx_register(const X509_STORE *ctx, struct Curl_easy *data, bool crl_advisory,
                          bool crl_allow_no_crl, bool crl_disk_caching, bool crl_memory_caching,
                          long crl_download_timeout, long crl_download_max_size)
{
  struct store_ctx_entry *existing;

  existing = sctx_lookup(ctx);
  if (existing) {
    existing->data = data;
    existing->crl_advisory = crl_advisory;
    existing->crl_allow_no_crl = crl_allow_no_crl;
    existing->crl_disk_caching = crl_disk_caching;
    existing->crl_memory_caching = crl_memory_caching;
    existing->crl_download_timeout = crl_download_timeout;
    existing->crl_download_max_size = crl_download_max_size;
    return;
  }
  
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
  sctx_registry.entries[sctx_registry.size].crl_download_max_size = crl_download_max_size;
  sctx_registry.size++;
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

/* The registry is shared by all handshakes and can be reallocated, so verify
 * callbacks work on a copy of their entry taken under the lock. */
static int sctx_get(const X509_STORE *ctx, struct store_ctx_entry *out)
{
  struct store_ctx_entry *entry;

  _mutex_lock(&crl_response_cache_mutex);
  entry = sctx_lookup(ctx);
  if (entry)
    *out = *entry;
  _mutex_unlock(&crl_response_cache_mutex);
  return entry != NULL;
}

static void sctx_set_crl_num(const X509_STORE *ctx, int crl_num)
{
  struct store_ctx_entry *entry;

  _mutex_lock(&crl_response_cache_mutex);
  entry = sctx_lookup(ctx);
  if (entry)
    entry->curr_crl_num = crl_num;
  _mutex_unlock(&crl_response_cache_mutex);
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
  ucrl_registry.entries[ucrl_registry.size].uri = OPENSSL_malloc(PATH_MAX);
  if (ucrl_registry.entries[ucrl_registry.size].uri)
  {
    strncpy(ucrl_registry.entries[ucrl_registry.size].uri, uri, PATH_MAX);
    ucrl_registry.entries[ucrl_registry.size].uri[PATH_MAX - 1] = 0;
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

static void ucrl_remove_at(size_t i)
{
  X509_CRL_free(ucrl_registry.entries[i].crl);
  OPENSSL_free(ucrl_registry.entries[i].uri);

  ucrl_registry.entries[i] = ucrl_registry.entries[ucrl_registry.size - 1];
  ucrl_registry.size--;

  if (ucrl_registry.size == 0) {
    ucrl_clear();
  }
}

static void ucrl_unregister(const char *uri)
{
  if (!ucrl_registry.entries || !uri) {
    return;
  }

  for (size_t i = 0; i < ucrl_registry.size; ++i) {
    if (strcmp(ucrl_registry.entries[i].uri, uri) == 0) {
      ucrl_remove_at(i);
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
      c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
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
    if (data)
      failf(data, "Failed to create %s directory. Ignored. Error: %d",
            dir, errno);
    return NULL;
  }
  if (data)
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
  strncpy(cache_dir, home_env, PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/.cache", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/snowflake", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/crls", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/", PATH_MAX);
#elif defined(__APPLE__)
  char *home_env = getenv("HOME");
  if (home_env == NULL) {
    return NULL;
  }
  strncpy(cache_dir, home_env, PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/Library", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/Caches", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/Snowflake", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/crls", PATH_MAX);
  if (mkdir_if_not_exists(data, cache_dir) == NULL)
  {
    return NULL;
  }
  strncat(cache_dir, "/", PATH_MAX);
#elif  defined(_WIN32)
  char *home_env = getenv("LOCALAPPDATA");
  if (home_env == NULL) {
    return NULL;
  }
  strncat(cache_dir, home_env, PATH_MAX);
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

  env_dir = getenv(SF_CRL_RESPONSE_CACHE_DIR_ENV);
  if (data)
    infof(data, "CRL cache directory from environment: %s", env_dir ? env_dir : "(not set)");
  if (env_dir) {
    strncpy(cache_dir, env_dir, PATH_MAX);
#if defined(_WIN32)
    const size_t len = strnlen(cache_dir, PATH_MAX);
    if (cache_dir[len-1] != '\\') {
      strncat(cache_dir, "\\", PATH_MAX);
    }
#else
    const size_t len = strnlen(cache_dir, PATH_MAX);
    if (cache_dir[len-1] != '/') {
      strncat(cache_dir, "/", PATH_MAX);
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
    char file_name[PATH_MAX] = {0};
    strncpy(file_name, uri, PATH_MAX);
    normalize_filename(file_name);
    strncat(file_path, file_name, PATH_MAX);
  }
}

static void save_crl_in_memory(const struct store_ctx_entry *data, const char *uri,
                               X509_CRL **pcrl, time_t download_time)
{
  if (!data->crl_memory_caching)
    return;

  ucrl_unregister(uri);
  ucrl_register(uri, *pcrl, download_time);
}

static void save_crl_to_disk(const struct store_ctx_entry *data, const char *uri,
                             X509_CRL **pcrl)
{
  BIO *fp;
  char file_path[PATH_MAX] = {0};

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
                              X509_CRL **pcrl, time_t *download_time)
{
  BIO *fp;
  char file_path[PATH_MAX] = {0};

  if (!data->crl_disk_caching) {
    infof(data->data, "CRL disk caching is disabled. Not loading CRL from disk (URI: %s)", uri);
    return;
  }

  infof(data->data, "CRL disk caching is enabled. Loading CRL from disk (URI: %s)", uri);

  // lookup for CRL on disk
  get_file_path_by_uri(data, uri, file_path);
  if (*file_path) {
    fp = BIO_new_file(file_path, "r");
    if (fp) {
      struct sf_stat file_stats;
      long fd = BIO_get_fd(fp, NULL);
      if (fd != -1) {
        if (sf_fstat(fd, &file_stats) == 0) {
          *pcrl = PEM_read_bio_X509_CRL(fp, NULL, NULL, NULL);
          if (*pcrl && download_time)
            *download_time = file_stats.st_mtime;
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
    time_t disk_time = time(NULL);
    get_crl_from_disk(data, uri, pcrl, &disk_time);
    if (*pcrl) {
      save_crl_in_memory(data, uri, pcrl, disk_time);
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
  save_crl_in_memory(data, uri, &crl, time(NULL));

  _mutex_unlock(&crl_response_cache_mutex);
}

static X509_CRL *load_crl(struct store_ctx_entry *data, const char *uri)
{
  X509_CRL *crl_cached = NULL;
  X509_CRL *crl = NULL;

  if (get_crl_from_cache(data, uri, &crl_cached))
    return crl_cached;

  long max_size = data->crl_download_max_size > 0
                    ? data->crl_download_max_size
                    : SF_CRL_DOWNLOAD_MAX_SIZE;

  BIO *mem = OSSL_HTTP_get(uri, NULL /* proxy */, NULL /* no_proxy */,
                           NULL, NULL, NULL /* cb */, NULL /* arg */,
                           1024 /* buf_size */, NULL /* headers */,
                           NULL /* expected_ct */, 1 /* expect_asn1 */,
                           max_size, data->crl_download_timeout);

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
  const X509_STORE *store = X509_STORE_CTX_get0_store(ctx);
  struct store_ctx_entry entry;
  struct store_ctx_entry *data = &entry;

  if (!sctx_get(store, data))
    return NULL;

  sctx_set_crl_num(store, 0);

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

  sctx_set_crl_num(store, sk_X509_CRL_num(crls));
  return crls;
}

static int error_handler(int ok, X509_STORE_CTX *ctx)
{
  int err;
  X509 *err_cert;
  struct store_ctx_entry entry;
  struct store_ctx_entry *data = &entry;
#define MAX_CERT_NAME_LEN 64
  char X509_cert_name[MAX_CERT_NAME_LEN + 1];

  if (ok)
    return ok;

  if (!sctx_get(X509_STORE_CTX_get0_store(ctx), data))
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

static long parse_env_seconds(const char *name, long default_val)
{
  const char *v = getenv(name);
  char *end = NULL;
  long parsed;

  if (!v || !*v)
    return default_val;
  parsed = strtol(v, &end, 10);
  if (end == v || *end != '\0' || parsed < 0)
    return default_val;
  return parsed;
}

/* Disk cache files are the URI with : / \\ etc. replaced by '_', so
 * "http://example.com/crl" becomes "http___example.com_crl". */
static int is_cached_crl_filename(const char *name)
{
  static const char *const prefixes[] = {
    "http___", "https___", "ldap___", "ldaps___"
  };
  size_t i;

  if (!name)
    return 0;
  for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
    if (strncmp(name, prefixes[i], strlen(prefixes[i])) == 0)
      return 1;
  }
  return 0;
}

static int should_remove_cached_crl_file(const char *file_path, time_t threshold)
{
  BIO *fp;
  X509_CRL *crl;
  const ASN1_TIME *next_update;
  int cmp;

  fp = BIO_new_file(file_path, "r");
  if (!fp)
    return 0;

  crl = PEM_read_bio_X509_CRL(fp, NULL, NULL, NULL);
  BIO_free(fp);
  if (!crl)
    return 1;

  next_update = X509_CRL_get0_nextUpdate(crl);
  if (!next_update) {
    X509_CRL_free(crl);
    return 1;
  }

  cmp = ASN1_TIME_cmp_time_t(next_update, threshold);
  X509_CRL_free(crl);
  /* OpenSSL returns -2 when the comparison fails; keep the file. */
  if (cmp == -2)
    return 0;
  return cmp <= 0;
}

struct crl_cleanup_settings {
  long validity;
  long removal_delay;
  char cache_dir[PATH_MAX];
};

/* Used by the cleanup thread, which must not call getenv(): it is not safe
 * while another thread modifies the environment. Each registerCRLCheck()
 * reads the environment and updates them, under crl_response_cache_mutex. */
static struct crl_cleanup_settings crl_cleanup_thread_settings;
/* Written only while no cleanup thread is running. */
static long crl_cleanup_thread_interval;

static void read_cleanup_settings(const struct Curl_easy *data,
                                  struct crl_cleanup_settings *settings)
{
  settings->validity = parse_env_seconds(SF_CRL_CACHE_VALIDITY_TIME_ENV,
                                         SF_CRL_CACHE_VALIDITY_TIME_DEFAULT);
  settings->removal_delay =
    parse_env_seconds(SF_CRL_ON_DISK_CACHE_REMOVAL_DELAY_ENV,
                      SF_CRL_ON_DISK_CACHE_REMOVAL_DELAY_DEFAULT);
  get_cache_dir(data, settings->cache_dir);
}

static void cleanup_memory_cache_locked(long validity)
{
  time_t now = time(NULL);
  size_t i = 0;

  while (i < ucrl_registry.size) {
    struct uri_crl_entry *entry = &ucrl_registry.entries[i];
    const int expired = is_crl_expired(entry->crl);
    const int evicted = (entry->download_time + validity) < now;
    if (expired || evicted)
      ucrl_remove_at(i);
    else
      i++;
  }
}

static void cleanup_disk_cache(long removal_delay, const char *cache_dir)
{
  time_t now = time(NULL);
  time_t threshold = now - removal_delay;

  if (!*cache_dir)
    return;

#ifdef _WIN32
  {
    char search_path[PATH_MAX];
    WIN32_FIND_DATA find_data;
    HANDLE handle;

    strncpy(search_path, cache_dir, PATH_MAX - 1);
    search_path[PATH_MAX - 1] = 0;
    strncat(search_path, "*", PATH_MAX);
    handle = FindFirstFile(search_path, &find_data);
    if (handle == INVALID_HANDLE_VALUE)
      return;

    do {
      char file_path[PATH_MAX];

      if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        continue;
      if (!is_cached_crl_filename(find_data.cFileName))
        continue;

      strncpy(file_path, cache_dir, PATH_MAX - 1);
      file_path[PATH_MAX - 1] = 0;
      strncat(file_path, find_data.cFileName, PATH_MAX);
      if (should_remove_cached_crl_file(file_path, threshold))
        DeleteFile(file_path);
    } while (FindNextFile(handle, &find_data));

    FindClose(handle);
  }
#else
  {
    DIR *dir = opendir(cache_dir);
    struct dirent *entry;

    if (!dir)
      return;

    while ((entry = readdir(dir)) != NULL) {
      char file_path[PATH_MAX];

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      if (!is_cached_crl_filename(entry->d_name))
        continue;

      strncpy(file_path, cache_dir, PATH_MAX - 1);
      file_path[PATH_MAX - 1] = 0;
      strncat(file_path, entry->d_name, PATH_MAX);
      if (should_remove_cached_crl_file(file_path, threshold))
        unlink(file_path);
    }

    closedir(dir);
  }
#endif
}

static void cleanup_cache(const struct crl_cleanup_settings *settings)
{
  _mutex_lock(&crl_response_cache_mutex);
  if (crl_cache_initialized) {
    cleanup_memory_cache_locked(settings->validity);
    cleanup_disk_cache(settings->removal_delay, settings->cache_dir);
  }
  _mutex_unlock(&crl_response_cache_mutex);
}

SF_PUBLIC(void) cleanupCertCRLCache(void)
{
  struct crl_cleanup_settings settings;

  if (!crl_response_cache_mutex_created)
    return;

  read_cleanup_settings(NULL, &settings);
  cleanup_cache(&settings);
}

static void *crl_cleanup_thread_proc(void *arg)
{
  const long interval = crl_cleanup_thread_interval;
  long slept;

  (void)arg;
  cleanup_cache(&crl_cleanup_thread_settings);

  while (!crl_cleanup_stop && interval > 0) {
    slept = 0;
    while (slept < interval && !crl_cleanup_stop) {
      _sleep_sec();
      slept++;
    }
    if (!crl_cleanup_stop)
      cleanup_cache(&crl_cleanup_thread_settings);
  }

  return NULL;
}

/* Called under the lock. Not left to the fork handlers: a fork can skip them,
 * and registering them can fail. */
static void forget_inherited_cleanup_thread(void)
{
#ifndef _WIN32
  if (crl_cleanup_thread_attempted && crl_cleanup_thread_pid != getpid()) {
    crl_cleanup_thread_started = 0;
    crl_cleanup_thread_attempted = 0;
  }
#endif
}

static void start_crl_cleanup_thread(void)
{
  long interval;

  forget_inherited_cleanup_thread();
  if (crl_cleanup_thread_attempted || !crl_cache_initialized)
    return;
  /* A disabled or failed start is not retried on later connections. */
  crl_cleanup_thread_attempted = 1;
#ifndef _WIN32
  crl_cleanup_thread_pid = getpid();
  /* Without the fork handlers, a fork while the thread holds the cache lock
   * would leave the child unable to take it. */
  if (crl_atfork_error)
    return;
#endif

  interval = parse_env_seconds(SF_CRL_CACHE_CLEANUP_INTERVAL_ENV,
                               SF_CRL_CACHE_CLEANUP_INTERVAL_DEFAULT);
  if (interval <= 0)
    return;

  crl_cleanup_thread_interval = interval;
  crl_cleanup_stop = 0;
#ifdef _WIN32
  crl_cleanup_thread_started =
    _thread_init(&crl_cleanup_thread, crl_cleanup_thread_win, NULL) == 0;
#else
  crl_cleanup_thread_started =
    _thread_init(&crl_cleanup_thread, crl_cleanup_thread_proc, NULL) == 0;
#endif
}

#ifdef _WIN32
static DWORD WINAPI crl_cleanup_thread_win(LPVOID arg)
{
  crl_cleanup_thread_proc(arg);
  return 0;
}
#endif

static void term_crl(void)
{
  int join_thread;

  if (!crl_response_cache_mutex_created)
    return;

  /* Under the lock, so registerCRLCheck() cannot start a thread after the join. */
  _mutex_lock(&crl_response_cache_mutex);
  forget_inherited_cleanup_thread();
  join_thread = crl_cache_initialized && crl_cleanup_thread_started;
  crl_cache_initialized = 0;
  crl_cleanup_thread_started = 0;
  crl_cleanup_thread_attempted = 0;
  if (join_thread)
    crl_cleanup_stop = 1;
  _mutex_unlock(&crl_response_cache_mutex);

  /* Join the cleanup thread before OpenSSL/libcurl teardown. */
  if (join_thread)
    _thread_join(crl_cleanup_thread);

  _mutex_lock(&crl_response_cache_mutex);
  sctx_clear();
  ucrl_clear();
  _mutex_unlock(&crl_response_cache_mutex);
}

#ifndef _WIN32
/* A forked child must not inherit the cache mutex locked, or the cache
 * half-updated, by another thread. glibc removes the handlers when the shared
 * object containing this code is unloaded. On macOS they outlive dlclose(),
 * but OpenSSL, unless built with no-pinshared, keeps the shared object it is
 * linked into loaded. */
static pthread_once_t crl_atfork_once = PTHREAD_ONCE_INIT;
static int crl_atfork_locked = 0;

static void crl_atfork_prepare(void)
{
  crl_atfork_locked = crl_response_cache_mutex_created &&
                      pthread_mutex_lock(&crl_response_cache_mutex) == 0;
}

static void crl_atfork_unlock(void)
{
  if (crl_atfork_locked)
    pthread_mutex_unlock(&crl_response_cache_mutex);
}

/* Also covers a child whose pid matches a dead process that started a thread. */
static void crl_atfork_child(void)
{
  crl_cleanup_thread_started = 0;
  crl_cleanup_thread_attempted = 0;
  crl_atfork_unlock();
}

static void register_crl_atfork(void)
{
  crl_atfork_error =
    pthread_atfork(crl_atfork_prepare, crl_atfork_unlock, crl_atfork_child);
}
#endif

SF_PUBLIC(void) termCertCRL(void)
{
  term_crl();
}

SF_PUBLIC(int) isCertCRLCleanupThreadRunning(void)
{
  int running;

  if (!crl_response_cache_mutex_created)
    return 0;
  _mutex_lock(&crl_response_cache_mutex);
  forget_inherited_cleanup_thread();
  running = crl_cleanup_thread_started;
  _mutex_unlock(&crl_response_cache_mutex);
  return running;
}

SF_PUBLIC(void) registerCRLCheck(struct Curl_easy *data,
                                 X509_STORE *ctx,
                                 bool crl_advisory,
                                 bool crl_allow_no_crl,
                                 bool crl_disk_caching,
                                 bool crl_memory_caching,
                                 long crl_download_timeout,
                                 long crl_download_max_size)
{
  struct crl_cleanup_settings settings;
  infof(data, "Registering SF CRL Validation...");
#ifndef _WIN32
  if (crl_atfork_error)
    infof(data, "CRL fork handlers not registered (error %d); the CRL cache "
          "cleanup thread is disabled and a forked child may inherit the CRL "
          "cache lock", crl_atfork_error);
#endif

  read_cleanup_settings(data, &settings);
  if (*settings.cache_dir)
    infof(data, "CRL cache file directory: %s", settings.cache_dir);
  else
    infof(data, "CRL cache file directory not exists!");


  /* register handler to read CRLs */
  X509_STORE_set_lookup_crls_cb(ctx, lookup_crls_handler);

  /* register handler to skip CRL errors when needed */
  X509_STORE_set_verify_cb(ctx, error_handler);

  X509_STORE_set_flags(ctx, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL | X509_V_FLAG_X509_STRICT);

  _mutex_lock(&crl_response_cache_mutex);
  /* register X509_STORE with given parameters */
  sctx_register(ctx, data,
               crl_advisory, crl_allow_no_crl, crl_disk_caching, crl_memory_caching,
               crl_download_timeout, crl_download_max_size);
  crl_cleanup_thread_settings = settings;

  /* Started on first use, not in initCertCRL(): a process that initializes
   * curl and then forks (e.g. a PHP-FPM master) must not own the thread. */
  start_crl_cleanup_thread();
  _mutex_unlock(&crl_response_cache_mutex);
}

SF_PUBLIC(void) initCertCRL()
{
  /* not thread safe; called from curl_global_init() */
  if (!crl_response_cache_mutex_created)
    crl_response_cache_mutex_created = _mutex_init(&crl_response_cache_mutex) == 0;
  _mutex_lock(&crl_response_cache_mutex);
  crl_cache_initialized = 1;
  _mutex_unlock(&crl_response_cache_mutex);
#ifndef _WIN32
  pthread_once(&crl_atfork_once, register_crl_atfork);
#endif
  atexit(term_crl);
}
