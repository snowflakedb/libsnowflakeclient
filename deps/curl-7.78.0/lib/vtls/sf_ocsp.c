/*
* Copyright (c) 2017-2019 Snowflake Computing
*/

#include "curl_setup.h"

#include "urldata.h"
#include "sendf.h"
#include "formdata.h" /* for the boundary function */
#include "url.h" /* for the ssl config check function */
#include "inet_pton.h"
#include "openssl.h"
#include "connect.h"
#include "slist.h"
#include "select.h"
#include "vtls.h"
#include "strcase.h"
#include "hostcheck.h"
#include "curl_printf.h"
#include "sf_ocsp.h"
#include "sf_cJSON.h"
#include <openssl/ssl.h>
#include <openssl/ocsp.h>
#include <openssl/asn1.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include "oobtelemetry.h"

#ifdef __linux__
#include <linux/limits.h>
#endif

#ifdef _WIN32
// Windows
#include <windows.h>
#include <direct.h>
#include <time.h>

typedef HANDLE SF_THREAD_HANDLE;
typedef CONDITION_VARIABLE SF_CONDITION_HANDLE;
typedef CRITICAL_SECTION SF_CRITICAL_SECTION_HANDLE;
typedef SRWLOCK SF_RWLOCK_HANDLE;
typedef HANDLE SF_MUTEX_HANDLE;

#define PATH_MAX MAX_PATH  /* Maximum PATH legnth */
#define F_OK    0          /* Test for existence.  */
#define PATH_SEP "\\"
#else
// Linux and MacOSX
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>

typedef pthread_t SF_THREAD_HANDLE;
typedef pthread_cond_t SF_CONDITION_HANDLE;
typedef pthread_mutex_t SF_CRITICAL_SECTION_HANDLE;
typedef pthread_rwlock_t SF_RWLOCK_HANDLE;
typedef pthread_mutex_t SF_MUTEX_HANDLE;
#define PATH_SEP "/"
#endif

#include "curl_base64.h"
#include "curl_memory.h"
#include "memdebug.h"
#include "sf_ocsp_telemetry_data.h"

#define DEFAULT_OCSP_RESPONSE_CACHE_HOST "http://ocsp.snowflakecomputing.com"
#define OCSP_RESPONSE_CACHE_JSON "ocsp_response_cache.json"
#define OCSP_RESPONSE_CACHE_URL "%s/%s"
#define OCSP_RESPONDER_RETRY_URL "http://ocsp.snowflakecomputing.com/retry"

#define GET_STR_OCSP_LOG(X,Y) X->Y ? cJSON_CreateString(X->Y) : NULL
#define GET_BOOL_OCSP_LOG(X,Y) X->Y ? cJSON_CreateString("True") : cJSON_CreateString("False")
#define FREE_OCSP_LOG(X) if (X) free(X)

// Connection timeout in seconds for CA OCSP Responder
#define CA_OCSP_RESPONDER_CONNECTION_TIMEOUT 10L

// Connection timeout in seconds for OCSP Cache Server
#define OCSP_CACHE_SERVER_CONNECTION_TIMEOUT 5L

// Max number of connection retry attempts for OCSP Responder in Fail Open Mode
#define CA_OCSP_RESPONDER_MAX_RETRY_FO 1

// Max number of connection retry attempts for OCSP Responder in Fail Close Mode
#define CA_OCSP_RESPONDER_MAX_RETRY_FC 3

// Max number of connection retry attempts for OCSP Cache Server
#define OCSP_CACHE_SERVER_MAX_RETRY 1

typedef enum
{
    INVALID,
    VALID
}SF_OCSP_STATUS;

typedef enum
{
  DISABLED = 0,
  ENABLED
}SF_FAILOPEN_STATUS;

typedef enum
{
  TEST_DISABLED = 0,
  TEST_ENABLED
}SF_TESTMODE_STATUS;

typedef enum
{
  CERT_STATUS_GOOD,
  CERT_STATUS_REVOKED,
  CERT_STATUS_UNKNOWN,
  CERT_STATUS_INVALID,
  CERT_STATUS_UNAVAILABLE
}SF_CERT_STATUS;

typedef enum
{
  SF_OCSP_TEST_MODE,
  SF_TEST_OCSP_CACHE_SERVER_CONNECTION_TIMEOUT,
  SF_TEST_OCSP_FORCE_BAD_RESPONSE_VALIDITY,
  SF_TEST_CA_OCSP_RESPONDER_CONNECTION_TIMEOUT,
  SF_TEST_OCSP_CERT_STATUS_REVOKED,
  SF_TEST_OCSP_CERT_STATUS_UNKNOWN
}SF_OCSP_TEST;

/* private function declarations */
static char *ossl_strerror(unsigned long error, char *buf, size_t size);
static SF_CERT_STATUS checkResponse(OCSP_RESPONSE *resp,
                                    STACK_OF(X509) *ch,
                                    X509_STORE *st,
                                    struct Curl_easy *data,
                                    SF_OTD *ocsp_log_data);
static int prepareRequest(OCSP_REQUEST **req,
                   OCSP_CERTID *id,
                   struct Curl_easy *data);
static void updateOCSPResponseInMem(OCSP_CERTID *id, OCSP_RESPONSE *resp,
                             struct Curl_easy *data);
static OCSP_RESPONSE * findOCSPRespInMem(OCSP_CERTID *certid,
                                         struct Curl_easy *data,
                                         SF_OTD *ocsp_log_data);
static OCSP_RESPONSE * getOCSPResponse(X509 *cert, X509 *issuer,
							struct connectdata *conn, struct Curl_easy *data,
							SF_FAILOPEN_STATUS ocsp_fail_open, SF_OTD *ocsp_log);
static CURLcode checkOneCert(X509 *cert, X509 *issuer,
                      STACK_OF(X509) *ch, X509_STORE *st,
                      SF_FAILOPEN_STATUS ocsp_fail_open,
                      struct connectdata *conn,
					  struct Curl_easy *data,
                      SF_OTD *ocsp_log_data);
static char* ensureCacheDir(char* cache_dir, struct Curl_easy* data);
static char* mkdirIfNotExists(char* dir, struct Curl_easy* data);
static void writeOCSPCacheFile(struct Curl_easy* data);
static void readOCSPCacheFile(struct Curl_easy* data, SF_OTD *ocsp_log_data);
static OCSP_RESPONSE * queryResponderUsingCurl(char *url, OCSP_CERTID *certid,
                                        char *hostname, OCSP_REQUEST *req,
                                        struct Curl_easy *data,
                                                SF_FAILOPEN_STATUS ocsp_fail_open,
                                                SF_OTD *ocsp_log_data);
static void initOCSPCacheServer(struct Curl_easy *data);
static void downloadOCSPCache(struct Curl_easy *data, SF_OTD *ocsp_log_data);
static char* encodeOCSPCertIDToBase64(OCSP_CERTID *certid, struct Curl_easy *data);
static char* encodeOCSPRequestToBase64(OCSP_REQUEST *reqp, struct Curl_easy *data);
static char* encodeOCSPResponseToBase64(OCSP_RESPONSE* resp, struct Curl_easy *data);
static OCSP_CERTID* decodeOCSPCertIDFromBase64(char* src, struct Curl_easy *data);
static OCSP_RESPONSE* decodeOCSPResponseFromBase64(char* src, struct Curl_easy *data);
static SF_OCSP_STATUS checkResponseTimeValidity(OCSP_RESPONSE *resp,
                                                struct Curl_easy *data,
                                                SF_OTD *ocsp_log_data);
static OCSP_RESPONSE* extractOCSPRespFromValue(cJSON *cache_value,
                                               struct Curl_easy *data,
                                               SF_OTD *ocsp_log_dataa);
static cJSON *getCacheEntry(OCSP_CERTID* certid, struct Curl_easy *data);
static void deleteCacheEntry(OCSP_CERTID* certid, struct Curl_easy *data);
static void updateCacheWithBulkEntries(cJSON* tmp_cache, struct Curl_easy *data);
static char * getOCSPPostReqData(const char *hname, OCSP_CERTID *cid,
                                 const char * ocsp_url, const char *ocsp_req,
                                 struct Curl_easy *data);
static int checkSSDStatus(void);
static void termCertOCSP();
static void printOCSPFailOpenWarning(SF_OTD *ocsp_log, struct Curl_easy *data);
static char * generateOCSPTelemetryData(SF_OTD *ocsp_log);
static void clearOSCPLogData(SF_OTD *ocsp_log);
static SF_TESTMODE_STATUS getTestStatus(SF_OCSP_TEST test_name);

static int _mutex_init(SF_MUTEX_HANDLE *lock);
static int _mutex_lock(SF_MUTEX_HANDLE *lock);
static int _mutex_unlock(SF_MUTEX_HANDLE *lock);
static int _mutex_term(SF_MUTEX_HANDLE *lock);

/* in memory response cache */
static cJSON * ocsp_cache_root = NULL;

/* mutex for ocsp_cache_root */
static SF_MUTEX_HANDLE ocsp_response_cache_mutex;

/** OCSP Cache Server is used if enabled */
static int ocsp_cache_server_enabled = 0;

static char* ocsp_cache_server_url_env = NULL;

static char ocsp_cache_server_url[4096] = "";

static char ocsp_cache_server_retry_url_pattern[4096];

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

#ifdef _WIN32
/** start to sleep for 1s before retrying (milliseconds) */
static const long START_SLEEP_TIME = 1000;

    /** max sleep time before retrying (milliseconds) */
static const long MAX_SLEEP_TIME = 16000;
#else
/** start to sleep for 1s before retrying (microseconds) */
static const unsigned int START_SLEEP_TIME = 1000000;

/** max sleep time before retrying (microseconds) */
static const unsigned int MAX_SLEEP_TIME = 16000000;
#endif
static const int MAX_RETRY = 1;


/* Return error string for last OpenSSL error
 */
static char *ossl_strerror(unsigned long error, char *buf, size_t size)
{
  ERR_error_string_n(error, buf, size);
  return buf;
}

static SF_TESTMODE_STATUS getTestStatus(SF_OCSP_TEST test_name) {
  char *env = NULL;
  switch(test_name)
  {
    case SF_OCSP_TEST_MODE:
      env = getenv("SF_OCSP_TEST_MODE");
      if (env != NULL)
      {
        return TEST_ENABLED;
      }
      else
      {
        return TEST_DISABLED;
      }
      break;
    case SF_TEST_OCSP_CACHE_SERVER_CONNECTION_TIMEOUT:
      env = getenv("SF_TEST_OCSP_CACHE_SERVER_CONNECTION_TIMEOUT");
      if (env != NULL)
      {
        return atol(env);
      }
      else
      {
        return 0;
      }
      break;
    case SF_TEST_OCSP_FORCE_BAD_RESPONSE_VALIDITY:
      env = getenv("SF_TEST_OCSP_FORCE_BAD_RESPONSE_VALIDITY");
      if (env != NULL)
      {
        return TEST_ENABLED;
      }
      else
      {
        return TEST_DISABLED;
      }
      break;
    case SF_TEST_CA_OCSP_RESPONDER_CONNECTION_TIMEOUT:
      env = getenv("SF_TEST_CA_OCSP_RESPONDER_CONNECTION_TIMEOUT");
      if (env != NULL)
      {
        return atol(env);
      }
      else
      {
        return 0;
      }
      break;
    case SF_TEST_OCSP_CERT_STATUS_REVOKED:
      env = getenv("SF_TEST_OCSP_CERT_STATUS_REVOKED");
      if (env != NULL)
      {
        return TEST_ENABLED;
      }
      else
      {
        return TEST_DISABLED;
      }
      break;
    case SF_TEST_OCSP_CERT_STATUS_UNKNOWN:
      env = getenv("SF_TEST_OCSP_CERT_STATUS_UNKNOWN");
      if (env != NULL)
      {
        return TEST_ENABLED;
      }
      else
      {
        return TEST_DISABLED;
      }
      break;
    default:
      return TEST_DISABLED;
  }
}

static void printTestWarning(struct Curl_easy *data) {
  infof(data, "WARNING - DRIVER CONFIGURED IN TEST MODE");
}

static int checkSSDStatus(void) {
    char *ssd_env = getenv("SF_OCSP_ACTIVATE_SSD");
    int i = 0;

    if (!ssd_env)
    {
        return 0;
    }

    for(i = 0; ssd_env[i] != '\0'; i++)
    {
        ssd_env[i] = (char)tolower(ssd_env[i]);
    }
    return (strncmp(ssd_env, "true", sizeof("true")) == 0);
}


SF_CERT_STATUS checkResponse(OCSP_RESPONSE *resp,
                             STACK_OF(X509) *ch,
                             X509_STORE *st,
                             struct Curl_easy *data,
                             SF_OTD *ocsp_log_data)
{
  int i;
  SF_CERT_STATUS result = CERT_STATUS_INVALID;
  OCSP_BASICRESP *br = NULL;
  int ocsp_res = 0;
  char error_buffer[1024];

  br = OCSP_response_get1_basic(resp);
  if (getTestStatus(SF_OCSP_TEST_MODE) == TEST_ENABLED)
  {
    printTestWarning(data);
    if (getTestStatus(SF_TEST_OCSP_FORCE_BAD_RESPONSE_VALIDITY) == TEST_ENABLED)
    {
      failf(data, "Forced Failure - Bad OCSP Response.");
      result = CERT_STATUS_INVALID;
      goto end;
    }
  }

  if (br == NULL)
  {
    failf(data, "Failed to get OCSP response basic");
    sf_otd_set_event_sub_type(OCSP_RESPONSE_LOAD_FAILURE, ocsp_log_data);
    result = CERT_STATUS_INVALID;
    goto end;
  }

  ocsp_res = OCSP_basic_verify(br, ch, st, 0);
  /* ocsp_res: 1... success, 0... error, -1... fatal error */
  if (ocsp_res <= 0)
  {
    failf(data,
          "OCSP response signature verification failed. ret: %s",
          ossl_strerror(ERR_get_error(), error_buffer,
                        sizeof(error_buffer)));
    sf_otd_set_event_sub_type(OCSP_RESPONSE_SIGNATURE_INVALID, ocsp_log_data);
    result = CERT_STATUS_INVALID;
    goto end;
  }
  infof(data,
        "OCSP response signature verification success. ret: %s",
        ossl_strerror(ERR_get_error(), error_buffer,
                      sizeof(error_buffer)));

  for (i = 0; i < OCSP_resp_count(br); i++)
  {
    int cert_status, crl_reason;
    int psec, pday;
    long skewInSec = 900L;

    OCSP_SINGLERESP *single = NULL;

    ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;

    single = OCSP_resp_get0(br, i);
    if(!single)
      continue;

    cert_status = OCSP_single_get0_status(single, &crl_reason, &rev,
                                          &thisupd, &nextupd);


    if (ASN1_TIME_diff(&pday, &psec, thisupd, nextupd))
    {
      /*
       * Allow leeway = 1% of the difference of OCSP response validity
       * validity = 1% (actual validity of OCSP Response)
       */
      long validity = (pday*24*60*60 + psec)/100;
      skewInSec = validity > skewInSec ? validity : skewInSec;
      infof(data, "Diff between thisupd and nextupd "
              "day: %d, sec: %d, Tolerant skew: %d", pday, psec,
              skewInSec);
    }
    else
    {
      failf(data, "Invalid structure of ASN1_GENERALIZEDTIME");
      result = CERT_STATUS_INVALID;
      goto end;
    }

    /*
     * Consider the OCSP response' time range to be valid if it lies
     * in the following range - [thisupd-validity, nextupd+validity]
     */
    if(cert_status != V_OCSP_CERTSTATUS_REVOKED &&
          !OCSP_check_validity(thisupd, nextupd, skewInSec, -1L)) {
      failf(data, "OCSP response has expired");
      sf_otd_set_event_sub_type(OCSP_RESPONSE_EXPIRED, ocsp_log_data);
      result = CERT_STATUS_INVALID;
      goto end;
    }

    if (getTestStatus(SF_OCSP_TEST_MODE) == TEST_ENABLED)
    {
      printTestWarning(data);
      if (getTestStatus(SF_TEST_OCSP_CERT_STATUS_REVOKED) == TEST_ENABLED)
      {
        failf(data, "Forced Failure - REVOKED OCSP Status.");
        cert_status = V_OCSP_CERTSTATUS_REVOKED;
      }
      else if (getTestStatus(SF_TEST_OCSP_CERT_STATUS_UNKNOWN) == TEST_ENABLED)
      {
        failf(data, "Forced Failure - UNKNOWN OCSP Status");
        cert_status = V_OCSP_CERTSTATUS_UNKNOWN;
      }
    }

    switch(cert_status)
    {
      case V_OCSP_CERTSTATUS_GOOD:
        result = CERT_STATUS_GOOD;
        infof(data, "SSL certificate status: %s (%d)",
              OCSP_cert_status_str(cert_status), cert_status);
        goto end;

      case V_OCSP_CERTSTATUS_REVOKED:
        result = CERT_STATUS_REVOKED;
        failf(data, "SSL certificate revocation reason: %s (%d)",
              OCSP_crl_reason_str(crl_reason), crl_reason);
        goto end;

      case V_OCSP_CERTSTATUS_UNKNOWN:
        result = CERT_STATUS_UNKNOWN;
        infof(data, "SSL certificate status: %s (%d)",
              OCSP_cert_status_str(cert_status), cert_status);
        goto end;
    }
  }

end:
  OCSP_BASICRESP_free(br);
  return result;
}

struct curl_read_data
{
  char * read_data_ptr;
  int size_left;
};

struct curl_memory_write
{
  char *memory_ptr;
  size_t size;
};

static size_t
write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct curl_memory_write *mem = (struct curl_memory_write *)userp;

  mem->memory_ptr = realloc(mem->memory_ptr, mem->size + realsize + 1);

  memcpy(&(mem->memory_ptr[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory_ptr[mem->size] = 0; /* null terminated */

  return realsize;
}

static char * getOCSPPostReqData(const char *hname,
        OCSP_CERTID *cid,
                                 const char * ocsp_url, const char *ocsp_req,
                                 struct Curl_easy *data)
{
    char *ret_data = NULL;
    cJSON *hostname = cJSON_CreateString(hname);
    cJSON *ocsp_request = cJSON_CreateString(ocsp_req);
    cJSON *cert_id = cJSON_CreateString(encodeOCSPCertIDToBase64(cid, data));
    cJSON *ocsp_responder_url = cJSON_CreateString(ocsp_url);

    cJSON *ocsp_post = cJSON_CreateObject();

    if (!ocsp_post)
    {
        goto end;
    }

    cJSON_AddItemToObject(ocsp_post, "cert_id", cert_id);
    cJSON_AddItemToObject(ocsp_post, "ocsp_request", ocsp_request);
    cJSON_AddItemToObject(ocsp_post, "ocsp_responder_url",ocsp_responder_url);
    cJSON_AddItemToObject(ocsp_post, "hostname", hostname);
    ret_data = cJSON_PrintUnformatted(ocsp_post);
    end:
    return ret_data;
}

static OCSP_RESPONSE * queryResponderUsingCurl(char *url, OCSP_CERTID *certid, char *hostname,
                                        OCSP_REQUEST *req, struct Curl_easy *data,
                                                SF_FAILOPEN_STATUS ocsp_fail_open,
                                                SF_OTD *ocsp_log_data)
{
  unsigned char *ocsp_req_der = NULL;
  int len_ocsp_req_der = 0;
  OCSP_RESPONSE * ocsp_response = NULL;
  struct curl_memory_write ocsp_response_raw;
  unsigned char *ocsp_response_der = NULL;
  CURL *ocsp_curl = NULL;
  char *ocsp_req_base64 = NULL;
  size_t ocsp_req_base64_len = 0;
  CURLcode result;
  int use_ssl;
  char *host = NULL, *port = NULL, *path = NULL;
  char urlbuf[4096];
  int ocsp_retry_cnt = 0;

  char *ocsp_post_data = NULL;
  int ACTIVATE_SSD = checkSSDStatus();
  char *cert_id_b64 = encodeOCSPCertIDToBase64(certid, data);
  long sf_timeout = CA_OCSP_RESPONDER_CONNECTION_TIMEOUT;
  int test_timeout;
  char* test_ocsp_url = NULL;
  int max_retry = CA_OCSP_RESPONDER_MAX_RETRY_FO;
  int curl_success = 0;

  ocsp_response_raw.memory_ptr = malloc(1);  /* will be grown as needed by the realloc above */
  ocsp_response_raw.size = 0;
  // Update OCSP Log Data
  sf_otd_set_sfc_peer_host(hostname, ocsp_log_data);
  sf_otd_set_certid(cert_id_b64, ocsp_log_data);

  len_ocsp_req_der = i2d_OCSP_REQUEST(req, &ocsp_req_der);
  if (len_ocsp_req_der<= 0 || ocsp_req_der == NULL)
  {
    failf(data, "Failed to encode ocsp request into DER format");
    sf_otd_set_event_sub_type(OCSP_RESPONSE_LOAD_FAILURE, ocsp_log_data);
    goto end;
  }

  result = Curl_base64_encode(
      data, (char *)ocsp_req_der, (size_t)len_ocsp_req_der,
      &ocsp_req_base64, &ocsp_req_base64_len);
  if (result != CURLE_OK)
  {
    failf(data, "Failed to encode ocsp requst into base64 format");
    sf_otd_set_event_sub_type(OCSP_RESPONSE_ENCODE_FAILURE, ocsp_log_data);
    goto end;
  }

  sf_otd_set_ocsp_request(ocsp_req_base64, ocsp_log_data);

  if (getTestStatus(SF_OCSP_TEST_MODE) == TEST_ENABLED)
  {
    printTestWarning(data);
    if ((test_timeout = getTestStatus(SF_TEST_CA_OCSP_RESPONDER_CONNECTION_TIMEOUT)))
    {
      sf_timeout = test_timeout;
    }
    if ((test_ocsp_url = getenv("SF_TEST_OCSP_URL")) != NULL)
    {
      snprintf(urlbuf, sizeof(urlbuf),
              "%s", test_ocsp_url);
    }
  }

  OCSP_parse_url(url, &host, &port, &path, &use_ssl);
  /* build a direct OCSP URL or OCSP Dynamic Cache Server URL */
  if (getTestStatus(SF_OCSP_TEST_MODE) == TEST_DISABLED || test_ocsp_url == NULL)
  {
    if (!ACTIVATE_SSD)
    {
      snprintf(urlbuf, sizeof(urlbuf),
               ocsp_cache_server_retry_url_pattern,
               host, ocsp_req_base64);
    }
    else
    {
      snprintf(urlbuf, sizeof(urlbuf),
               ocsp_cache_server_retry_url_pattern);
    }
  }

  OPENSSL_free(host);
  OPENSSL_free(port);
  OPENSSL_free(path);

  infof(data, "OCSP fetch URL: %s", urlbuf);

  /* allocate another curl handle for ocsp checking */
  ocsp_curl = curl_easy_init();

  if (ocsp_curl)
  {
    curl_easy_setopt(ocsp_curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(ocsp_curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(ocsp_curl, CURLOPT_TIMEOUT, sf_timeout);
    curl_easy_setopt(ocsp_curl, CURLOPT_URL, urlbuf);
    curl_easy_setopt(ocsp_curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(ocsp_curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(ocsp_curl, CURLOPT_WRITEDATA, &ocsp_response_raw);

    if (ACTIVATE_SSD)
    {
        curl_easy_setopt(ocsp_curl, CURLOPT_POST, 1L);
        ocsp_post_data = getOCSPPostReqData(hostname, certid, url, ocsp_req_base64, data);
        if (!ocsp_post_data)
        {
            goto end;
        }
        curl_easy_setopt(ocsp_curl, CURLOPT_POSTFIELDS, ocsp_post_data);
    }

    ocsp_retry_cnt = 0;

    if (ocsp_fail_open == DISABLED)
    {
      max_retry = CA_OCSP_RESPONDER_MAX_RETRY_FC;
    }

    while(ocsp_retry_cnt < max_retry)
    {
      char error_msg[4096];
      CURLcode res = curl_easy_perform(ocsp_curl);

      if (res != CURLE_OK)
      {
        failf(data, "OCSP checking curl_easy_perform() failed: %s",
              curl_easy_strerror(res));
        if (ocsp_retry_cnt == max_retry -1)
        {
          snprintf(error_msg, OCSP_TELEMETRY_ERROR_MSG_MAX_LEN,
                  "OCSP checking curl_easy_perform() failed: %s\n",
                  curl_easy_strerror(res));
          sf_otd_set_error_msg(error_msg, ocsp_log_data);
          sf_otd_set_event_sub_type(OCSP_RESPONSE_FETCH_FAILURE, ocsp_log_data);
        }
        ocsp_retry_cnt++;
      }
      else if (res == CURLE_OK)
      {
        long response_code;
        curl_easy_getinfo(ocsp_curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code >= 200 && response_code < 300)
        {
          infof(data, "OCSP request returned with http status code 2XX");
          curl_success = 1;
          break;
        }
        else if (response_code < 200 || response_code >= 300)
        {
          failf(data, "OCSP request failed with non-200 level code: %d",
                response_code);

          if (ocsp_retry_cnt == max_retry-1)
          {
            snprintf(error_msg, OCSP_TELEMETRY_ERROR_MSG_MAX_LEN,
                "OCSP request failed with non 200 level code %li", response_code);
            sf_otd_set_error_msg(error_msg, ocsp_log_data);
            sf_otd_set_event_sub_type(OCSP_RESPONSE_FETCH_FAILURE, ocsp_log_data);
          }
          ocsp_retry_cnt++;
        }
      }
    }
  }

  if (!curl_success)
  {
    goto end;
  }

  ocsp_response_der = (unsigned char*)ocsp_response_raw.memory_ptr;
  ocsp_response = d2i_OCSP_RESPONSE(NULL,
                                    (const unsigned char**)&ocsp_response_der,
                                    (long)ocsp_response_raw.size);
  if (ocsp_response == NULL)
  {
    failf(data, "Failed to decode ocsp response");
    sf_otd_set_error_msg("Failed to decode ocsp response", ocsp_log_data);
    sf_otd_set_event_sub_type(OCSP_RESPONSE_DECODE_FAILURE, ocsp_log_data);
  }

end:
  if (ocsp_req_base64) curl_free(ocsp_req_base64);
  if (cert_id_b64) curl_free(cert_id_b64);
  if (ocsp_req_der) OPENSSL_free(ocsp_req_der);
  if (ocsp_curl) curl_easy_cleanup(ocsp_curl);

  free(ocsp_response_raw.memory_ptr);

  return ocsp_response;
}

/**
 * Prepares OCSP request
 * @param req OCSP_REQUEST
 * @param certid OCSP_CERTID
 * @param data curl handle
 * @return 1 if success otherwise 0
 */
int prepareRequest(OCSP_REQUEST **req,
                   OCSP_CERTID *certid,
                   struct Curl_easy *data)
{
  if(!*req) *req = OCSP_REQUEST_new();
  if(!*req)
  {
    failf(data, "Failed to allocate OCSP request");
    goto err;
  }
  if(!OCSP_request_add0_id(*req, certid))
  {
    failf(data, "Failed to attach CertID to OCSP request");
      goto err;
  }
  return 1;

err:
  return 0;
}

/**
 * Update the cache entry with the CertID and Response
 * @param certid OCSP_CERTID
 * @param resp OCSP_RESPONSE
 * @param data curl handle
 */
void updateOCSPResponseInMem(OCSP_CERTID *certid, OCSP_RESPONSE *resp,
                             struct Curl_easy *data)
{
  char * cert_id_encode= NULL;
  char * ocsp_response_encode = NULL;

  unsigned long unix_time;

  cJSON * cache_val_array = NULL;
  cJSON *found = NULL;

  /* encode OCSP CertID and OCSP Response */
  cert_id_encode = encodeOCSPCertIDToBase64(certid, data);
  if (cert_id_encode == NULL)
  {
    failf(data, "UpdateOCSPResponseInMem - cert id encode failed");
    goto end;
  }
  ocsp_response_encode = encodeOCSPResponseToBase64(resp, data);
  if (ocsp_response_encode == NULL)
  {
    failf(data, "UpdateOCSPResponseInMem - ocsp response encode failed %s", cert_id_encode);
    goto end;
  }

  /* timestamp */
  unix_time = (unsigned long)time(NULL);

  /* write to mem cache */
  cache_val_array = cJSON_CreateArray();
  cJSON_AddItemToArray(cache_val_array, cJSON_CreateNumber((double) unix_time));
  cJSON_AddItemToArray(cache_val_array, cJSON_CreateString(ocsp_response_encode));

  _mutex_lock(&ocsp_response_cache_mutex);
  if (ocsp_cache_root == NULL)
  {
    ocsp_cache_root = cJSON_CreateObject();
  }
  found = getCacheEntry(certid, data);
  if (found != NULL)
  {
    /* delete existing entry first if exists. */
    cJSON_DeleteItemFromObject(ocsp_cache_root, found->string);
  }
  cJSON_AddItemToObject(ocsp_cache_root, cert_id_encode, cache_val_array);
  _mutex_unlock(&ocsp_response_cache_mutex);
end:
  if (cert_id_encode) curl_free(cert_id_encode);
  if (ocsp_response_encode) curl_free(ocsp_response_encode);
}

/**
 * Encode OCSP CertID to Base 64 string
 *
 * @param certid OCSP_CERTID
 * @param data curl handle
 * @return base64 string
 */
char* encodeOCSPCertIDToBase64(OCSP_CERTID *certid, struct Curl_easy *data)
{
  int len;
  char* ret = NULL;
  unsigned char *der_buf = NULL;
  size_t encode_len = 0;
  CURLcode result;

  len = i2d_OCSP_CERTID(certid, &der_buf);
  if (len <= 0 || der_buf == NULL)
  {
    infof(data, "Failed to encode OCSP CertId");
    goto end;
  }
  result = Curl_base64_encode(data,(char *)der_buf, (size_t)len,
                              &ret, &encode_len);
  if (result != CURLE_OK)
  {
    infof(data, "Failed to encode OCSP CertId to base64: %s",
          curl_easy_strerror(result));
  }
end:
  if (der_buf) OPENSSL_free(der_buf);
  return ret;
}

/**
 * Encode OCSP Request to Base 64 string
 * @param reqp OCSP_REQUEST
 * @param data curl handle
 * @return base64 string
 */
char *encodeOCSPRequestToBase64(OCSP_REQUEST *reqp, struct Curl_easy *data)
{
  int len;
  char* ret = NULL;
  unsigned char *der_buf = NULL;
  size_t encode_len = 0;
  CURLcode result;

  len = i2d_OCSP_REQUEST(reqp, &der_buf);
  if (len <= 0 || der_buf == NULL)
  {
    infof(data, "Failed to encode OCSP response");
    goto end;
  }

  result = Curl_base64_encode(data, (char *)der_buf, (size_t)len,
                              &ret, &encode_len);
  if (result != CURLE_OK)
  {
    infof(data, "Failed to encode OCSP response to base64: %s",
          curl_easy_strerror(result));
    goto end;
  }
end:
  if (der_buf) OPENSSL_free(der_buf);
  return ret;
}

/**
 * Encode OCSP Response to Base 64 string
 *
 * @param resp OCSP_RESPONSE
 * @param data curl handle
 * @return base64 string
 */
char *encodeOCSPResponseToBase64(OCSP_RESPONSE* resp, struct Curl_easy *data)
{
  int len;
  char* ret = NULL;
  unsigned char *der_buf = NULL;
  size_t encode_len = 0;
  CURLcode result;

  len = i2d_OCSP_RESPONSE(resp, &der_buf);
  if (len <= 0 || der_buf == NULL)
  {
    infof(data, "Failed to encode OCSP response");
    goto end;
  }

  result = Curl_base64_encode(data, (char *)der_buf, (size_t)len,
                              &ret, &encode_len);
  if (result != CURLE_OK)
  {
    infof(data, "Failed to encode OCSP response to base64: %s",
          curl_easy_strerror(result));
    goto end;
  }
end:
  if (der_buf) OPENSSL_free(der_buf);
  return ret;
}

/**
 * Decoede OCSP CertID from Base 64 string
 * @param src a base64 string
 * @param data curl handle
 * @return OCSP_CERTID
 */
OCSP_CERTID* decodeOCSPCertIDFromBase64(char* src, struct Curl_easy *data)
{
  unsigned char *ocsp_certid_der = NULL;
  unsigned char *ocsp_certid_der_org = NULL;
  size_t ocs_certid_der_len;
  OCSP_CERTID *target_certid = NULL;
  CURLcode result;
  if (src == NULL)
  {
    infof(data, "Base64 input is NULL for decoding OCSP CertID");
    return NULL;
  }

  result = Curl_base64_decode(src,
                              &ocsp_certid_der, &ocs_certid_der_len);
  if (result != CURLE_OK)
  {
    infof(data, "Failed to decode OCSP CertID in the cache. Ignored: %s",
          curl_easy_strerror(result));
    return NULL;
  }
  /* keep the original pointer as it will be updated in d2i function */
  ocsp_certid_der_org = ocsp_certid_der;

  target_certid = d2i_OCSP_CERTID(
    NULL, (const unsigned char**)&ocsp_certid_der, (long)ocs_certid_der_len);

  curl_free(ocsp_certid_der_org);

  if (target_certid == NULL)
  {
    infof(data, "Failed to decode OCSP CertID.");
    return NULL;
  }
  return target_certid;
}

/**
 * Decode OCSP Response from Base 64 string
 * @param src a base64 string
 * @param data curl handle
 * @return OCSP_RESPONSE or NULL
 */
OCSP_RESPONSE * decodeOCSPResponseFromBase64(char* src, struct Curl_easy *data)
{
  OCSP_RESPONSE *resp = NULL;
  unsigned char *ocsp_response_der;
  unsigned char *ocsp_response_der_org;
  size_t ocsp_response_der_len;
  CURLcode result;

  if (src == NULL)
  {
    infof(data, "Base64 input is NULL for decoding OCSP Response");
    return NULL;
  }
  result = Curl_base64_decode(src,
                              &ocsp_response_der, &ocsp_response_der_len);
  if (result)
  {
    infof(data, "Failed to decode OCSP response from base64 string: %s",
          curl_easy_strerror(result));
    return NULL;
  }
  /* keep the original pointer as it will be updated in d2i function */
  ocsp_response_der_org = ocsp_response_der;

  resp = d2i_OCSP_RESPONSE(NULL, (const unsigned char **) &ocsp_response_der,
                           (long) ocsp_response_der_len);
  curl_free(ocsp_response_der_org);
  if (resp == NULL)
  {
    infof(data, "Failed to decode OCSP response cache from der format");
    return NULL;
  }
  return resp;
}



/**
 * Validate OCSP Response time validity
 */

static SF_OCSP_STATUS checkResponseTimeValidity(OCSP_RESPONSE *resp, struct Curl_easy *data, SF_OTD *ocsp_log_data)
{
    int i;
    SF_OCSP_STATUS ret_val = INVALID;
    OCSP_BASICRESP *br = NULL;

    br = OCSP_response_get1_basic(resp);
    if (br == NULL)
    {
        failf(data, "Failed to get OCSP response basic from cache");
        sf_otd_set_event_sub_type(OCSP_RESPONSE_CACHE_ENTRY_LOAD_FAILED, ocsp_log_data);
        goto end;
    }

    for (i = 0; i < OCSP_resp_count(br); i++)
    {
        int crl_reason;
        int psec, pday;
        long skewInSec = 900L;

        OCSP_SINGLERESP *single = NULL;

        ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;

        single = OCSP_resp_get0(br, i);
        if(!single)
            continue;

        OCSP_single_get0_status(single, &crl_reason, &rev,
                                &thisupd, &nextupd);


        if (ASN1_TIME_diff(&pday, &psec, thisupd, nextupd))
        {
            /*
             * Allow leeway = 1% of the difference of OCSP response validity
             * validity = 1% (actual validity of OCSP Response)
             */
            long validity = (pday*24*60*60 + psec)/100;
            skewInSec = validity > skewInSec ? validity : skewInSec;
            infof(data, "Diff between thisupd and nextupd "
            "day: %d, sec: %d, Tolerant skew: %d", pday, psec,
            skewInSec);
        }
        else
        {
            failf(data, "Invalid structure of ASN1_GENERALIZEDTIME");
            goto end;
        }
        /*
         * Consider the OCSP response' time range to be valid if it lies
         * in the following range - [thisupd-validity, nextupd+validity]
         */
        if(!OCSP_check_validity(thisupd, nextupd, skewInSec, -1L))
        {
          failf(data, "OCSP response has expired");
          sf_otd_set_event_sub_type(OCSP_RESPONSE_FROM_CACHE_EXPIRED, ocsp_log_data);
          goto end;
        }
    }

    ret_val = VALID;

    end:
    OCSP_BASICRESP_free(br);
    return ret_val;
}


/**
 * Extract OCSP Response from the cache value.
 *
 * Must mutex protected as it may delete the existing element if the last
 * query time is older than 24 hours.
 *
 * @param cache_value a cache value to update
 * @param data curl handle
 * @return OCSP_RESPONSE
 */
OCSP_RESPONSE* extractOCSPRespFromValue(cJSON *cache_value, struct Curl_easy *data,
                                        SF_OTD *ocsp_log_data)
{
  long last_query_time_l = 0L;
  cJSON * resp_bas64_j = NULL;
  cJSON * last_query_time = NULL;
  OCSP_RESPONSE *resp = NULL;

  if (cache_value == NULL || !cJSON_IsArray(cache_value))
  {
    infof(data, "OCSP Cache value is invalid");
    goto end;
  }

  /* First item is the timestamp when the cache entry was created. */
  last_query_time = cJSON_GetArrayItem(cache_value, 0);
  if (!cJSON_IsNumber(last_query_time))
  {
    infof(data, "OCSP Cache Last query time is invalid");
    cJSON_DeleteItemFromObjectCaseSensitive(ocsp_cache_root,
                                            cache_value->string);
    goto end;
  }

  last_query_time_l = (long)last_query_time->valuedouble;

  /* valid for 120 hours */
  if ((unsigned long)time(NULL) - last_query_time_l >= 24*60*60*5)
  {
    infof(data, "OCSP Response Cache Expired");
    goto end;
  }

  /* Second item is the actual OCSP response data */
  resp_bas64_j = cJSON_GetArrayItem(cache_value, 1);
  if (!cJSON_IsString(resp_bas64_j) || resp_bas64_j->valuestring == NULL)
  {
    infof(data, "OCSP Response cache is invalid. Deleting it from the cache.");
    cJSON_DeleteItemFromObjectCaseSensitive(ocsp_cache_root,
                                            cache_value->string);
    goto end;
  }

  /* decode OCSP Response from base64 string */
  resp = decodeOCSPResponseFromBase64(resp_bas64_j->valuestring, data);
  if (checkResponseTimeValidity(resp, data, ocsp_log_data) == INVALID)
  {
      resp = NULL;
  }
  end:
  return resp;
}

/**
 * Find OCSP Response in memory cache.
 * @param certid OCSP CertID
 * @param data curl handle
 * @return OCSP Response if success otherwise NULL
 */
OCSP_RESPONSE * findOCSPRespInMem(OCSP_CERTID *certid, struct Curl_easy *data, SF_OTD *ocsp_log_data)
{
  /* calculate certid */
  OCSP_RESPONSE *resp = NULL;
  cJSON *found = NULL;

  _mutex_lock(&ocsp_response_cache_mutex);
  found = getCacheEntry(certid, data);
  if (!found)
  {
    infof(data, "OCSP Response not found in the cache");
    goto end;
  }
  resp = extractOCSPRespFromValue(found, data, ocsp_log_data);
  if (resp)
  {
    /* NOTE encode to base64 only for logging */
    char* cert_id_encode = encodeOCSPCertIDToBase64(certid, data);
    infof(data, "OCSP Response Cache found!!!: %s", cert_id_encode);
    if (cert_id_encode) curl_free(cert_id_encode);
  }
end:
  _mutex_unlock(&ocsp_response_cache_mutex);
  return resp;
}

/**
 * Get the cache entry for the CertID.
 *
 * @param certid OCSP CertID
 * @param data curl handle
 * @return cJSON cache entry if found otherwise NULL
 */
cJSON *getCacheEntry(OCSP_CERTID* certid, struct Curl_easy *data)
{
  cJSON *ret = NULL;
  cJSON *element_pointer = NULL;

  if (ocsp_cache_root == NULL)
  {
    ocsp_cache_root = cJSON_CreateObject();
  }

  cJSON_ArrayForEach(element_pointer, ocsp_cache_root)
  {
    int cmp = 0;
    OCSP_CERTID *target_certid = decodeOCSPCertIDFromBase64(
      element_pointer->string, data);

    if (target_certid == NULL)
    {
      infof(data, "Failed to decode OCSP CertID in the cache.");
      continue;
    }
    cmp = OCSP_id_cmp(target_certid, certid);
    OCSP_CERTID_free(target_certid);
    if (cmp == 0)
    {
      ret = element_pointer;
      break;
    }
  }
  return ret;
}

/**
 * Delete a Cache entry for Cert ID
 * @param certid OCSP_CERTID
 * @param data curl handle
 */
void deleteCacheEntry(OCSP_CERTID* certid, struct Curl_easy *data)
{
  cJSON *found = NULL;

  _mutex_lock(&ocsp_response_cache_mutex);
  found = getCacheEntry(certid, data);
  if (found)
  {
    cJSON_DeleteItemFromObject(ocsp_cache_root, found->string);
  }
  _mutex_unlock(&ocsp_response_cache_mutex);
}

/**
 * Update OCSP cache with the cJSON data
 * @param tmp_cache a cJSON data
 * @param data curl handle
 */
void updateCacheWithBulkEntries(cJSON* tmp_cache, struct Curl_easy *data)
{
  cJSON *element_pointer = NULL;
  cJSON *found = NULL;
  cJSON *new_value = NULL;

  /* Detect the existing elements */
  cJSON_ArrayForEach(element_pointer, tmp_cache)
  {
    OCSP_CERTID *cert_id = decodeOCSPCertIDFromBase64(
      element_pointer->string, data);
    if (cert_id == NULL)
    {
      infof(data, "CertID is NULL");
      continue;
    }
    found = getCacheEntry(cert_id, data);
    OCSP_CERTID_free(cert_id);
    new_value = cJSON_Duplicate(element_pointer, 1);
    if (found != NULL)
    {
      cJSON_DeleteItemFromObject(ocsp_cache_root, found->string);
    }
    cJSON_AddItemToObject(ocsp_cache_root, element_pointer->string, new_value);
  }
}

/**
 * Download a OCSP cache content from OCSP cache server.
 * @param data curl handle
 */
void downloadOCSPCache(struct Curl_easy *data, SF_OTD *ocsp_log_data)
{
  struct curl_memory_write ocsp_response_cache_json_mem;
  CURL *curlh = NULL;
  struct curl_slist *headers = NULL;
  cJSON *tmp_cache = NULL;
  CURLcode res = CURLE_OK;

  int sf_timeout = OCSP_CACHE_SERVER_CONNECTION_TIMEOUT;
  int test_timeout = OCSP_CACHE_SERVER_CONNECTION_TIMEOUT;

  ocsp_response_cache_json_mem.memory_ptr = malloc(1);  /* will be grown as needed by the realloc above */
  ocsp_response_cache_json_mem.size = 0;


  /* allocate another curl handle for ocsp checking */
  curlh = curl_easy_init();
  if (!curlh)
  {
    failf(data, "Failed to initialize curl to download OCSP cache.");
    sf_otd_set_event_sub_type(OCSP_RESPONSE_CURL_FAILURE, ocsp_log_data);
    goto end;
  }

  headers = curl_slist_append(headers, "Content-Type:application/json");

  if (getTestStatus(SF_OCSP_TEST_MODE))
  {
    printTestWarning(data);
    if ((test_timeout = getTestStatus(SF_TEST_OCSP_CACHE_SERVER_CONNECTION_TIMEOUT)))
    {
      failf(data, "Forced Failure - REVOKED OCSP Status.");
      sf_timeout = test_timeout;
    }
  }

  curl_easy_setopt(curlh, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curlh, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt(curlh, CURLOPT_TIMEOUT, sf_timeout);
  curl_easy_setopt(curlh, CURLOPT_URL, ocsp_cache_server_url);
  /* empty means use all of builtin supported encodings */
  curl_easy_setopt(curlh, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curlh, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curlh, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curlh, CURLOPT_WRITEDATA, &ocsp_response_cache_json_mem);

  res = CURLE_OK;

  res = curl_easy_perform(curlh);
  if (res == CURLE_OK)
  {
    long response_code;
    curl_easy_getinfo(curlh, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 200 && response_code < 300)
    {
      /* Success */
      infof(data, "OCSP Cache Download request returned with http status code 2XX");
      infof(data, "Successfully downloaded OCSP Cache.");
    }
    else if (response_code < 200 || response_code >= 300 )
    {
      failf(data, "OCSP cache download request failed with non-200 level code: %d, OCSP Cache Could not be downloaded",
            response_code);
      sf_otd_set_event_sub_type(OCSP_RESPONSE_CACHE_DOWNLOAD_FAILED, ocsp_log_data);
      goto end;
    }
  }
  else
  {
    failf(data, "CURL Response code is not CURLE_OK.");
    sf_otd_set_event_sub_type(OCSP_RESPONSE_CURL_FAILURE, ocsp_log_data);
    goto end;
  }

  _mutex_lock(&ocsp_response_cache_mutex);
  if (ocsp_cache_root == NULL)
  {
    ocsp_cache_root = cJSON_CreateObject();
  }
  tmp_cache = cJSON_Parse(ocsp_response_cache_json_mem.memory_ptr);

  /* update OCSP cache with the downloaded cache */
  updateCacheWithBulkEntries(tmp_cache, data);
  infof(data, "Number of cache entries: %d",
        cJSON_GetArraySize(ocsp_cache_root));

  _mutex_unlock(&ocsp_response_cache_mutex);
end:
  if (tmp_cache) cJSON_Delete(tmp_cache);
  if (curlh) curl_easy_cleanup(curlh);
  if (headers) curl_slist_free_all(headers);

  free(ocsp_response_cache_json_mem.memory_ptr);
}

/**
 * Find OCSP Response in local cache first. If not found, go to OCSP server to
 * get the response.

 * @param cert subject certificate
 * @param issuer issuer certificate
 * @param data curl connection data
 * @return OCSP response
 */
OCSP_RESPONSE * getOCSPResponse(X509 *cert, X509 *issuer,
                                struct connectdata *conn,
								struct Curl_easy *data,
                                SF_FAILOPEN_STATUS ocsp_fail_open,
                                SF_OTD *ocsp_log_data)
{
  int i;
  bool ocsp_url_missing = true;
  bool ocsp_url_invalid = true;
  OCSP_RESPONSE *resp = NULL;
  OCSP_REQUEST *req = NULL;
  OCSP_CERTID *certid = NULL;
  STACK_OF(OPENSSL_STRING) *ocsp_list = NULL;
  const EVP_MD *cert_id_md = EVP_sha1();

  /* get certid first */
  certid = OCSP_cert_to_id(cert_id_md, cert, issuer);
  resp = findOCSPRespInMem(certid, data, ocsp_log_data);
  if (resp != NULL)
  {
    sf_otd_set_cache_hit(1, ocsp_log_data);
    infof(data, "OCSP Entry found in cache");
    goto end; /* found OCSP response in cache */
  }

  if (ocsp_cache_server_enabled)
  {
    sf_otd_set_cache_enabled(1, ocsp_log_data);
    /* download OCSP Cache from the server*/
    downloadOCSPCache(data, ocsp_log_data);

    /* try hitting cache again */
    resp = findOCSPRespInMem(certid, data, ocsp_log_data);
    if (resp != NULL)
    {
      sf_otd_set_cache_hit(1, ocsp_log_data);
      infof(data, "OCSP entry found in cache after fresh cache download");
      goto end; /* found OCSP response in cache */
    }
  }
  else
  {
    sf_otd_set_cache_enabled(0, ocsp_log_data);
    infof(data, "OCSP Cache Server is disabled");
    ocsp_log_data->cache_enabled = 0;
  }

  sf_otd_set_cache_hit(0, ocsp_log_data);
  if (!prepareRequest(&req, certid, data))
  {
    sf_otd_set_event_sub_type(OCSP_REQUEST_CREATION_FAILURE, ocsp_log_data);
    goto end; /* failed to prepare request */
  }

  /* loop through OCSP urls */
  ocsp_list = X509_get1_ocsp(cert);
  for (i = 0; i < sk_OPENSSL_STRING_num(ocsp_list); i++)
  {
    int use_ssl;
    int url_parse_result;
    char *host = NULL, *port = NULL, *path = NULL;

    char *ocsp_url = sk_OPENSSL_STRING_value(ocsp_list, i);
    if (ocsp_url == NULL)
    {
      failf(data, "OCSP Validation URL is not present");
      /*
       * Try the next OCSP Server in ocsp_list, if present.
       */
      continue;
    }
    ocsp_url_missing = false;
    infof(data, "OCSP Validation URL: %s", ocsp_url);
    strncpy(ocsp_log_data->ocsp_responder_url, ocsp_url, sizeof(ocsp_log_data->ocsp_responder_url));
    url_parse_result = OCSP_parse_url(
        ocsp_url, &host, &port, &path, &use_ssl);
    if (!url_parse_result)
    {
      failf(data, "Invalid OCSP Validation URL: %s", ocsp_url);
      /* try the next OCSP server if available. Usually only one OCSP server
       * is attached, so it is unlikely the second OCSP server is used, but
       * in theory it could, so it let try. */
      continue;
    }

    ocsp_url_invalid = false;
    resp = queryResponderUsingCurl(ocsp_url, certid, conn->host.name, req, data, ocsp_fail_open, ocsp_log_data);
    /* update local cache */
    OPENSSL_free(host);
    OPENSSL_free(path);
    OPENSSL_free(port);

    updateOCSPResponseInMem(certid, resp, data);
    break; /* good if any OCSP server works */
  }

  if(ocsp_url_missing || ocsp_url_invalid)
  {
    sf_otd_set_event_sub_type(OCSP_URL_MISSING_OR_INVALID, ocsp_log_data);
  }

end:
  if (ocsp_list) X509_email_free(ocsp_list);
  /* when ocsp certid is found in mem, which means that no OCSP_REQUEST object
     will be created, so needs to explicitly freed certid. Otherwise, certid
     will be freed along with the deallocation of OCSP_REQUEST object */
  if (!req && certid) OCSP_CERTID_free(certid);
  /* https://www.openssl.org/docs/man1.1.0/crypto/OCSP_request_add0_id.html
   * certid must NOT be freed here */
  if (req) OCSP_REQUEST_free(req);
  return resp;
}


static void printOCSPFailOpenWarning(SF_OTD *ocsp_log, struct Curl_easy *data)
{
  char *ocsp_log_data = NULL;
  ocsp_log_data = generateOCSPTelemetryData(ocsp_log);
  infof(data, "WARNING!!! Using fail-open to connect. Driver is connecting to an "
              "HTTPS endpoint without OCSP based Certificate Revocation checking "
              "as it could not obtain a valid OCSP Response to use from the CA OCSP "
              "responder. Details:%s",ocsp_log_data);
  if (ocsp_log_data)
  {
    sendOOBevent(ocsp_log_data);
    if(ocsp_log_data) cJSON_free(ocsp_log_data);
  }
}

static char * generateOCSPTelemetryData(SF_OTD *ocsp_log)
{
  char *oobevent;
  if (!ocsp_log) {
    return NULL;
  }
  setOOBeventdata(OOBEVENTNAME, "OCSPException", 0);
  setOOBeventdata(URGENCY, NULL, 1);
  oobevent = prepareOOBevent(ocsp_log);
  clearOSCPLogData(ocsp_log);
  return oobevent;
}

static void clearOSCPLogData(SF_OTD *ocsp_log)
{
  if (!ocsp_log) {
    return;
  }
  ocsp_log->event_type[0] = '\0';
  ocsp_log->event_sub_type[0] = '\0';
  ocsp_log->sfc_peer_host[0] = '\0';
  ocsp_log->cert_id[0] = '\0';
  ocsp_log->ocsp_req_b64[0] = '\0';
  ocsp_log->ocsp_responder_url[0] = '\0';
  ocsp_log->error_msg[0] = '\0';
}
/**
 * Check one certificate
 * @param cert subject certificate
 * @param issuer issuer certificate
 * @param ch certificate chain
 * @param st CA certificates
 * @param conn curl connection data
 * @return curl return code
 */
CURLcode checkOneCert(X509 *cert, X509 *issuer,
                      STACK_OF(X509) *ch, X509_STORE *st,
                      SF_FAILOPEN_STATUS ocsp_fail_open,
                      struct connectdata *conn,
					  struct Curl_easy *data,
                      SF_OTD *ocsp_log_data)
{
  CURLcode result;
  SF_CERT_STATUS sf_cert_status = CERT_STATUS_INVALID;
#define MAX_CERT_NAME_LEN 100
  char X509_cert_name[MAX_CERT_NAME_LEN + 1];
  int retry_count = 0;
  OCSP_RESPONSE *resp = NULL;
  const EVP_MD *cert_id_md = EVP_sha1();

  char *ocsp_log_str = NULL;


  while(retry_count < MAX_RETRY)
  {
    OCSP_CERTID *certid = NULL;
    int ocsp_status = 0;
    resp = getOCSPResponse(cert, issuer, conn, data, ocsp_fail_open, ocsp_log_data);
    infof(data, "Starting checking cert for %s...",
          X509_NAME_oneline(X509_get_subject_name(cert), X509_cert_name,
                            MAX_CERT_NAME_LEN));
    if (!resp)
    {
      failf(data, "Unable to get OCSP response");
      if (retry_count == MAX_RETRY - 1)
      {
        sf_otd_set_event_sub_type(OCSP_RESPONSE_FETCH_FAILURE, ocsp_log_data);
      }
      sf_cert_status = CERT_STATUS_INVALID;
      ++retry_count;
      continue;
    }

    ocsp_status = OCSP_response_status(resp);
    if (ocsp_status != OCSP_RESPONSE_STATUS_SUCCESSFUL)
    {
      failf(data, "Invalid OCSP response status: %s (%d)",
            OCSP_response_status_str(ocsp_status), ocsp_status);
      if (retry_count ==  MAX_RETRY - 1)
      {
        sf_otd_set_event_sub_type(OCSP_RESPONSE_STATUS_UNSUCCESSFUL, ocsp_log_data);
      }
      sf_cert_status = CERT_STATUS_UNAVAILABLE;
      if (resp) OCSP_RESPONSE_free(resp);
      resp = NULL;
      ++retry_count;
      continue;
    }

    sf_cert_status = checkResponse(resp, ch, st, data, ocsp_log_data);

    if (sf_cert_status == CERT_STATUS_GOOD)
    {
      result = CURLE_OK;
      break;
    }
    else if (sf_cert_status == CERT_STATUS_REVOKED)
    {
      sf_otd_set_event_sub_type(OCSP_RESPONSE_CERT_STATUS_REVOKED, ocsp_log_data);
      break;
    }

    /* delete the cache if the validation failed. */
    certid = OCSP_cert_to_id(cert_id_md, cert, issuer);
    deleteCacheEntry(certid, data);
    OCSP_CERTID_free(certid);

    /* retry */
    ++retry_count;
  }

  if (sf_cert_status != CERT_STATUS_GOOD)
  {
    /* delete the cache if OCSP revocation check fails */
    OCSP_CERTID *certid = NULL;

    /* remove the entry from cache */
    certid = OCSP_cert_to_id(cert_id_md, cert, issuer);
    deleteCacheEntry(certid, data);
    OCSP_CERTID_free(certid);

    if (sf_cert_status == CERT_STATUS_UNAVAILABLE)
    {
      sf_otd_set_error_msg("OCSP Response Unavailable", ocsp_log_data);
      sf_otd_set_event_type("RevocationCheckFailure", ocsp_log_data);
      sf_otd_set_event_sub_type(OCSP_RESPONSE_CERT_STATUS_UNAVAILABLE, ocsp_log_data);
    }
    else if (sf_cert_status == CERT_STATUS_UNKNOWN)
    {
      sf_otd_set_error_msg("OCSP Response Unknown", ocsp_log_data);
      sf_otd_set_event_type("RevocationCheckFailure", ocsp_log_data);
      sf_otd_set_event_sub_type(OCSP_RESPONSE_CERT_STATUS_UNKNOWN, ocsp_log_data);
    }
    else if (sf_cert_status == CERT_STATUS_INVALID)
    {
      sf_otd_set_error_msg("OCSP Response is Invalid", ocsp_log_data);
      sf_otd_set_event_type("RevocationCheckFailure", ocsp_log_data);
      sf_otd_set_event_sub_type(OCSP_RESPONSE_CERT_STATUS_INVALID, ocsp_log_data);
    }

    if (ocsp_fail_open == ENABLED && sf_cert_status != CERT_STATUS_REVOKED)
    {
      sf_otd_set_fail_open_mode(1, ocsp_log_data);
      printOCSPFailOpenWarning(ocsp_log_data, data);
      result = CURLE_OK;
    }
    else
    {
      sf_otd_set_fail_open_mode(0, ocsp_log_data);
      result = CURLE_SSL_INVALIDCERTSTATUS;
      ocsp_log_str = generateOCSPTelemetryData(ocsp_log_data);
      if (ocsp_log_str)
      {
        sendOOBevent(ocsp_log_str);
        infof(data, ocsp_log_str);
        if(ocsp_log_str) cJSON_free(ocsp_log_str);
      }
    }
  }
  if (resp) OCSP_RESPONSE_free(resp);
  return result;
}

/**
 * Make a directory if not exists
 * @param dir a directory to create
 * @param data curl handle
 * @return directory name if success otherwise NULL
 */
char* mkdirIfNotExists(char* dir, struct Curl_easy *data)
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
    infof(data, "Already exists %s directory.", dir);
  }
  else
  {
    infof(data, "Created %s directory.", dir);
  }
  return dir;
}

/**
 * Ensure th cache directory is accessible
 * @param cache_dir cache directory
 * @param data curl handle
 * @return cache directory name or NULL
 */
char* ensureCacheDir(char* cache_dir, struct Curl_easy *data)
{
#ifdef __linux__
  char *home_env = getenv("HOME");
  strcpy(cache_dir, (home_env == NULL ? (char*)"/tmp" : home_env));

  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/.cache");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/snowflake");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
#elif defined(__APPLE__)
  char *home_env = getenv("HOME");
  strcpy(cache_dir, (home_env == NULL ? (char*)"/tmp" : home_env));
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/Library");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/Caches");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "/Snowflake");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
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
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\AppData");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\Local");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\Snowflake");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
  strcat(cache_dir, "\\Caches");
  if (mkdirIfNotExists(cache_dir, data) == NULL)
  {
    goto err;
  }
#endif
  infof(data, "OCSP cache file directory: %s", cache_dir);
  return cache_dir;
err:
  return NULL;
}

/**
 * Write OCSP cache onto a file in the cache directory
 * @param data curl handle
 */
void writeOCSPCacheFile(struct Curl_easy* data)
{
  char cache_dir[PATH_MAX] = "";
  char cache_file[PATH_MAX] = "";
  char cache_lock_file[PATH_MAX] = "";
  FILE *fh;
  FILE *fp;
  char * jsonText;

  _mutex_lock(&ocsp_response_cache_mutex);
  if (ocsp_cache_root == NULL)
  {
      infof(data, "Skipping writing OCSP cache file as no OCSP cache root exists.");
      goto end;
  }

  if (ensureCacheDir(cache_dir, data) == NULL)
  {
    failf(data, "The cache file is not accessible.");
    goto end;
  }

  /* cache file */
  strcpy(cache_file, cache_dir);
  strcat(cache_file, PATH_SEP);
  strcat(cache_file, OCSP_RESPONSE_CACHE_JSON);
  infof(data, "OCSP Cache file: %s", cache_file);

  /* cache lock directory/file */
  strcpy(cache_lock_file, cache_file);
  strcat(cache_lock_file, ".lck");

  if (access(cache_lock_file, F_OK) != -1)
  {
    /* lck file exists */
    struct stat statbuf;
    if (stat(cache_lock_file, &statbuf) != -1)
    {
      if ((long)time(NULL) - (long) statbuf.st_mtime < 60*60)
      {
        infof(data, "Other process lock the file, ignored");
        goto end;
      }
      else
      {
        infof(data, "Remove the old lock file");
        if (remove(cache_lock_file) != 0)
        {
          infof(data, "Failed to delete the lock file: %s, ignored", cache_lock_file);
          goto end;
        }
      }
    }
  }

  /* create a new lck file */
  fh = fopen(cache_lock_file, "w");
  if (fh == NULL)
  {
    infof(data, "Failed to create a lock file: %s. Skipping writing OCSP cache file.",
        cache_lock_file);
    goto end;
  }
  if (fclose(fh) != 0)
  {
    infof(data, "Failed to close a lock file: %s. Ignored.", cache_lock_file);
    goto end;
  }

  fp = fopen(cache_file, "w");
  if (fp == NULL)
  {
    infof(data, "Failed to open OCSP response cache file. Skipping writing OCSP cache file.");
    goto end;
  }
  jsonText = cJSON_PrintUnformatted(ocsp_cache_root);
  if (fprintf(fp, "%s", jsonText) < 0)
  {
    infof(data, "Failed to write OCSP response cache file. Skipping");
  }

  if (fclose(fp) != 0)
  {
    infof(data, "Failed to close OCSP response cache file: %s. Ignored", cache_file);
  }
  infof(data, "Write OCSP Response to cache file");

  /* deallocate json string */
  cJSON_free(jsonText);

  if (remove(cache_lock_file) != 0)
  {
    infof(data, "Failed to delete the lock file: %s, ignored", cache_lock_file);
  }
end:
  if (ocsp_cache_root != NULL)
  {
    cJSON_Delete(ocsp_cache_root);
    ocsp_cache_root = NULL;
  }
  _mutex_unlock(&ocsp_response_cache_mutex);
}

/**
 * Read OCSP cache from from the local cache directory
 * @param data curl handle
 */
void readOCSPCacheFile(struct Curl_easy* data, SF_OTD *ocsp_log_data)
{
  char cache_dir[PATH_MAX] = "";
  char cache_file[PATH_MAX] = "";
  char ocsp_resp_cache_str[20 * 1200]; /* TODO: malloc? */
  FILE* pfile = NULL;

  _mutex_lock(&ocsp_response_cache_mutex);

  if (ensureCacheDir(cache_dir, data) == NULL)
  {
    infof(data, "Could not ensure the presence of Cache Directory. "
                "Skipping reading cache. Driver will try to download"
                "the cache from the Cache Server directly.");
    sf_otd_set_event_sub_type(OCSP_CACHE_READ_FAILURE, ocsp_log_data);
    goto end;
  }

  /* cache file */
  strcpy(cache_file, cache_dir);
  strcat(cache_file, PATH_SEP);
  strcat(cache_file, OCSP_RESPONSE_CACHE_JSON);
  infof(data, "OCSP cache file: %s", cache_file);

  if (ocsp_cache_root != NULL)
  {
    infof(data, "OCSP cache was already read onto memory");
    goto end;
  }
  if (access(cache_file, F_OK) == -1)
  {
    infof(data, "No OCSP cache file found on disk. file: %s", cache_file);
    sf_otd_set_event_sub_type(OCSP_CACHE_READ_FAILURE, ocsp_log_data);
    goto end;
  }

  pfile = fopen(cache_file, "r");
  if (pfile == NULL)
  {
    infof(data, "Failed to open OCSP response cache file. Ignored.");
    sf_otd_set_event_sub_type(OCSP_CACHE_READ_FAILURE, ocsp_log_data);
    goto end;
  }

  if (fgets(ocsp_resp_cache_str, sizeof(ocsp_resp_cache_str), pfile) == NULL) {
    infof(data, "Failed to read OCSP response cache file. Ignored");
    goto file_close;
  }
  /* just attached the whole JSON object */
  ocsp_cache_root = cJSON_Parse(ocsp_resp_cache_str);
  if (ocsp_cache_root == NULL)
  {
    infof(data, "Failed to parse cache file content in json format");
    sf_otd_set_event_sub_type(OCSP_CACHE_READ_FAILURE, ocsp_log_data);
  }
  else
  {
    infof(data, "OCSP cache file was successfully loaded");
  }
file_close:
  if (fclose(pfile) != 0)
  {
    infof(data, "Failed to close cache file. Ignored.");
    goto end;
  }
end:
  if (ocsp_cache_root == NULL) ocsp_cache_root = cJSON_CreateObject();
  _mutex_unlock(&ocsp_response_cache_mutex);
  return;
}

/**
 * Initialize OCSP Cache Server
 * @param data curl handle
 */
void initOCSPCacheServer(struct Curl_easy *data)
{
  char* ocsp_cache_server_enabled_env = NULL;
  int ACTIVATE_SSD = checkSSDStatus();
  /* OCSP cache server URL */
  _mutex_lock(&ocsp_response_cache_mutex);
    ocsp_cache_server_enabled_env = getenv(
    "SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED");
  if (ocsp_cache_server_enabled_env == NULL ||
      strcmp(ocsp_cache_server_enabled_env, "false") != 0)
  {
    /* OCSP Cache server enabled by default */
    ocsp_cache_server_enabled = 1;
  }

  ocsp_cache_server_url_env = getenv(
    "SF_OCSP_RESPONSE_CACHE_SERVER_URL");

  // TEST ENABLED
  if (getTestStatus(SF_OCSP_TEST_MODE) == TEST_ENABLED)
  {
    char *test_cache_server_url = getenv("SF_TEST_OCSP_CACHE_SERVER_URL");
    printTestWarning(data);
    if (test_cache_server_url)
    {
      failf(data, "Forced Behavior - Cache Server URL.");
      snprintf(ocsp_cache_server_url, sizeof(ocsp_cache_server_url),
               "%s", test_cache_server_url);
      goto end;
    }
  }

  if (ocsp_cache_server_url_env == NULL)
  {
    /* default URL */
    snprintf(ocsp_cache_server_url, sizeof(ocsp_cache_server_url),
             OCSP_RESPONSE_CACHE_URL,
             DEFAULT_OCSP_RESPONSE_CACHE_HOST,
             OCSP_RESPONSE_CACHE_JSON);

    if (!ACTIVATE_SSD)
    {
      strncpy(ocsp_cache_server_retry_url_pattern,
              "%s/%s", sizeof(ocsp_cache_server_retry_url_pattern));
    }
    else
    {
      /*
       * Non private link customers always go to default
       * retry URL for OCSP retries
       */
      strncpy(ocsp_cache_server_retry_url_pattern,
              OCSP_RESPONDER_RETRY_URL,
              sizeof(OCSP_RESPONDER_RETRY_URL));
    }
  }
  else
  {
    // if specified, always use the special retry url
    int use_ssl;
    char *host = NULL, *port = NULL, *path = NULL;
    strncpy(ocsp_cache_server_url, ocsp_cache_server_url_env,
            sizeof(ocsp_cache_server_url));
    ocsp_cache_server_url[sizeof(ocsp_cache_server_url) - 1] = '\0';
    OCSP_parse_url(
            ocsp_cache_server_url_env, &host, &port, &path, &use_ssl);

    if (!ACTIVATE_SSD)
    {
      snprintf(ocsp_cache_server_retry_url_pattern,
               sizeof(ocsp_cache_server_retry_url_pattern),
               "%s://%s:%s/retry/%%s/%%s", use_ssl ? "https" : "http", host, port);
    }
    else
    {
      snprintf(ocsp_cache_server_retry_url_pattern,
               sizeof(ocsp_cache_server_retry_url_pattern),
               "%s://%s:%s/retry", use_ssl ? "https" : "http", host, port);
    }

    if (getTestStatus(SF_OCSP_TEST_MODE) == TEST_ENABLED)
    {
      char *test_cache_server_url = getenv("SF_TEST_OCSP_CACHE_SERVER_URL");
      printTestWarning(data);
      if (test_cache_server_url)
      {
        failf(data, "Forced Behavior - Cache Server URL.");
        snprintf(ocsp_cache_server_url, sizeof(ocsp_cache_server_url),
                 "%s", test_cache_server_url);
      }
    }

    OPENSSL_free(host);
    OPENSSL_free(path);
    OPENSSL_free(port);
  }

  end:
  infof(data, "SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED: %s",
    ocsp_cache_server_enabled ? "true" : "false");
  infof(data, "SF_OCSP_RESPONSE_CACHE_SERVER_URL: %s",
    ocsp_cache_server_url);
  _mutex_unlock(&ocsp_response_cache_mutex);
}

/**
 * Initialize Check OCSP revocation status method
 */
SF_PUBLIC(CURLcode) initCertOCSP()
{
  /* must call only once. not thread safe */
  _mutex_init(&ocsp_response_cache_mutex);
  atexit(termCertOCSP);
  return CURLE_OK;
}

/**
 * Terminate Check OCSP revocation status method
 */
static void termCertOCSP()
{
  /* terminate the mutex */
  _mutex_term(&ocsp_response_cache_mutex);
}

/**
 * Main entry point. Check OCSP revocation status.
 * @param conn Curl connection
 * @param ch a chain of certificates
 * @param st CA trust
 * @param ocsp_failopen_cnxn_param connection param setting for
 * failopen. This should be a binary value - either True or False
 * There is no option for this to be in a NOTSET state.
 * @return Curl return code
 */
SF_PUBLIC(CURLcode) checkCertOCSP(struct connectdata *conn,
                                  struct Curl_easy *data,
								  STACK_OF(X509) *ch,
								  X509_STORE *st,
								  int ocsp_failopen_cnxn_param)
{
  int numcerts;
  int i;
  CURLcode rs = CURLE_OK;
  char *ocsp_fail_open_env = getenv("SF_OCSP_FAIL_OPEN"); // Testing Only
  SF_FAILOPEN_STATUS ocsp_fail_open = ENABLED;


// These end points are Out of band telemetry end points.
// Do not use OCSP/failsafe on Out of band telemetry endpoints
  if ( (strcmp(conn->host.name, "sfctest.client-telemetry.snowflakecomputing.com") == 0 )
      || (strcmp(conn->host.name, "sfcdev.client-telemetry.snowflakecomputing.com") == 0 )
      || (strcmp(conn->host.name, "client-telemetry.snowflakecomputing.com") == 0 )
      ) {
    return rs;
  }

  SF_OTD ocsp_log_data;
  memset(&ocsp_log_data, 0, sizeof(SF_OTD));

  if (ocsp_fail_open_env != NULL)
  {
    /*
     * OCSP Fail Open Env Variable is for internal usage/ testing only.
     * Using it in production is not advised and not supported.
     */
    if(strncmp(ocsp_fail_open_env, "false", 5) != 0)
    {
      ocsp_fail_open = ENABLED;
    }
    else
    {
      ocsp_fail_open = DISABLED;
    }
  }
  else if (ocsp_failopen_cnxn_param)
  {
    ocsp_fail_open = ENABLED;
  }
  else
  {
    ocsp_fail_open = DISABLED;
  }

  sf_otd_set_insecure_mode(0, &ocsp_log_data);

  infof(data, "Cert Data Store: %s, Certifcate Chain: %s", st, ch);
  initOCSPCacheServer(data);

  infof(data, "Start SF OCSP Validation...");
  readOCSPCacheFile(data, &ocsp_log_data);

  numcerts = sk_X509_num(ch);
  infof(data, "Number of certificates in the chain: %d", numcerts);
  for (i =0; i < numcerts - 1; i++)
  {
    X509* cert = sk_X509_value(ch, i);
    X509* issuer = sk_X509_value(ch, i+1);

    rs = checkOneCert(cert, issuer, ch, st, ocsp_fail_open, conn, data, &ocsp_log_data);
    if (rs != CURLE_OK)
    {
      goto end;
    }
  }
  writeOCSPCacheFile(data);
end:
  infof(data, "End SF OCSP Validation... Result: %d", rs);
  return rs;
}
