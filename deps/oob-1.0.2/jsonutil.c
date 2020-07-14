#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "jsonutil.h"
#include "sf_cJSON.h"
#include "snowflake/Simba_CRTFunctionSafe.h"
#ifdef _WIN32
#include <combaseapi.h>
#include <objbase.h>
#define strcasecmp _stricmp
#else 
#include "uuid.h"
#endif

#ifndef TMP_BUF_LEN
#define TMP_BUF_LEN 40
#endif

static struct conStr connectionInfo = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0} };

static struct logDetails oobevent = {{0}, {0}, {0}, {0}, 0, 0};

cJSON* dsn = NULL;

// stores simba.snowflake.ini key value pairs
cJSON* simba = NULL;

void setdeployment(const char* host);

int sendOOBevent(char* event);

//connStr will be copied into another string
void setConnectionString(char const* connStr);

char* getConnectionString(void);

void gettime(char* buffer);

void getuuid(char* buffer);

char* getConnectionInfo(enum OOBINFO id);

void getCabundle(char* cabundle, int maxlen);

static void freeAll(void);

char* getOOBDeployment()
{
    return connectionInfo.dep;
}

void maskSecrets(char* str)
{
  //Look for Azure SAS key. 
  char *mask = strstr(str, "sig=");
  if(mask != NULL) {
    *str = 0;
  }
  return;
}

char* prepareOOBevent(oobOcspData* ocspevent)
{
  cJSON* root = NULL;
  cJSON* list = NULL;
  cJSON* tags = NULL;
  cJSON* vals = NULL;
  cJSON* key = NULL;
  char* str = NULL;
  char buffer[TMP_BUF_LEN]={0};
  char uuid[TMP_BUF_LEN]={0};
  char* driver_name = NULL;
  char* driver_version = NULL;

  root = cJSON_CreateArray();

  //Add list to the Array.
  list = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "list", list);

  gettime(buffer);
  key = cJSON_CreateString(buffer);
  if( ! key ) goto end;
  cJSON_AddItemToObject(list, "Created_On", key);

  if(ocspevent && ocspevent->event_type[0] != 0){
    key = cJSON_CreateString(ocspevent->event_type);
    if( ! key ) goto end;
    cJSON_AddItemToObject(list, "Name", key);
  }
  else if(oobevent.name[0] != 0){
    key = cJSON_CreateString(oobevent.name);
    if( ! key ) goto end;
    cJSON_AddItemToObject(list, "Name", key);
  }

  key = cJSON_CreateNumber(1);
  if( ! key ) goto end;
  cJSON_AddItemToObject(list, "SchemaVersion", key);

  tags = cJSON_CreateObject();
  if( ! tags ) goto end;
  cJSON_AddItemToObject(list, "Tags", tags);

  getuuid(uuid);
  key = cJSON_CreateString(uuid);
  if( ! key ) goto end;
  cJSON_AddItemToObject(tags, "UUID", key);

  if( connectionInfo.ctxStr[0] != 0 ){
    key = cJSON_CreateString( connectionInfo.ctxStr );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "connectionString", key);
  }
  else if(ocspevent && ocspevent->sfc_peer_host[0] != 0 ) {
    sb_sprintf(connectionInfo.ctxStr, sizeof(connectionInfo.ctxStr), "%s://%s:%s", connectionInfo.protocol, ocspevent->sfc_peer_host, connectionInfo.port);
    key = cJSON_CreateString(connectionInfo.ctxStr);
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "connectionString", key);
  }
  else if( connectionInfo.ctxStr[0] == 0 ){
    sb_sprintf(connectionInfo.ctxStr, sizeof(connectionInfo.ctxStr), "%s://%s:%s", connectionInfo.protocol, connectionInfo.host, connectionInfo.port);
    key = cJSON_CreateString( connectionInfo.ctxStr );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "connectionString", key);
  }

  if( connectionInfo.account[0] != 0 ){
    key = cJSON_CreateString( connectionInfo.account );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "ctx_account", key);
  }

  if( connectionInfo.host[0] != 0 ){
    key = cJSON_CreateString( connectionInfo.host );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "ctx_host", key);
  }

  if( connectionInfo.port[0] != 0 ){
    key = cJSON_CreateString( connectionInfo.port );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "ctx_port", key);
  }

  if( connectionInfo.protocol[0] != 0 ){
    key = cJSON_CreateString( connectionInfo.protocol );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "ctx_protocol", key);
  }

  if( connectionInfo.user[0] != 0 ){
    key = cJSON_CreateString( connectionInfo.user );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "ctx_user", key);
  }

  driver_name = getenv("SF_DRIVER_NAME");
  if (driver_name == NULL)
  {
    key = cJSON_CreateString("ODBC");
  }
  else
  {
    key = cJSON_CreateString(driver_name);
  }
  if( ! key ) goto end;
  cJSON_AddItemToObject(tags, "driver", key);

  driver_version = getenv("SF_DRIVER_VERSION");
  if (driver_version == NULL)
  {
    key = cJSON_CreateString("99.0.0");
  }
  else
  {
    key = cJSON_CreateString(driver_version);
  }
  if( ! key ) goto end;
  cJSON_AddItemToObject(tags, "version", key);

#if defined(__linux__) 
  key = cJSON_CreateString("Linux");
#elif defined(__APPLE__)
  key = cJSON_CreateString("Apple");
#elif defined(WIN32) 
  key = cJSON_CreateString("Win32");
#elif defined(_WIN64)
  key = cJSON_CreateString("Win64");
#endif
  if( ! key ) goto end;
  cJSON_AddItemToObject(tags, "hostOs", key);

  if(connectionInfo.dep[0] != 0) {
    key = cJSON_CreateString( connectionInfo.dep );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "telemetryServerDeployment", key);
  }else {
    key = cJSON_CreateString( "prod" );
    if( ! key ) goto end;
    cJSON_AddItemToObject(tags, "telemetryServerDeployment", key);
  }

  key = cJSON_CreateString("Log");
  if( ! key ) goto end;
  cJSON_AddItemToObject(list, "Type", key);

  key = cJSON_CreateString(uuid);
  if( ! key ) goto end;
  cJSON_AddItemToObject(list, "UUID", key);

  if(oobevent.urgent) {
    key = cJSON_CreateTrue();
  }
  else {
    key = cJSON_CreateFalse();
  }
  if( ! key ) goto end;
  cJSON_AddItemToObject(list, "Urgent", key);

  vals = cJSON_CreateObject();
  cJSON_AddItemToObject(list, "Value", vals);

  cJSON_AddItemToObject(vals, "DSN", dsn);
  cJSON_AddItemToObject(vals, "Simba", simba);

  if(ocspevent && ocspevent->sfc_peer_host[0] != 0 ) {
    key = cJSON_CreateString(ocspevent->sfc_peer_host);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "sfcPeerHost", key);
  }

  if(ocspevent && ocspevent->cert_id[0] != 0 ) {
    key = cJSON_CreateString(ocspevent->cert_id);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "certId", key);
  }

  if(ocspevent && ocspevent->ocsp_req_b64[0] != 0 ) {
    key = cJSON_CreateString(ocspevent->ocsp_req_b64);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "ocspRequestBase64", key);
  }

  if(ocspevent && ocspevent->ocsp_responder_url[0] != 0 ) {
    key = cJSON_CreateString(ocspevent->ocsp_responder_url);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "ocspResponderURL", key);
  }

  if(ocspevent && ocspevent->error_msg[0] != 0 ) {
    key = cJSON_CreateString(ocspevent->error_msg);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "errorMessage", key);
  }

  if(ocspevent ) {
    key = cJSON_CreateNumber(ocspevent->insecure_mode);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "insecureMode", key);
  }

  if(ocspevent ) {
    key = cJSON_CreateNumber(ocspevent->failopen_mode);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "failopenMode", key);
  }

  if(ocspevent ) {
    key = cJSON_CreateNumber(ocspevent->cache_enabled);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "cacheEnabled", key);
  }

  if(ocspevent ) {
    key = cJSON_CreateNumber(ocspevent->cache_hit);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "cacheHit", key);
  }

  if(oobevent.errorCode != 0){
    key = cJSON_CreateNumber( oobevent.errorCode);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "errorCode", key);
  }

  if(oobevent.responsestatuscode[0] != 0){
    key = cJSON_CreateString( oobevent.responsestatuscode);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "responsestatuscode", key);
  }

  if(oobevent.exceptionMessage[0] != 0){
    key = cJSON_CreateString( oobevent.exceptionMessage);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "exceptionMessage", key);
  }

  if(oobevent.exceptionStackTrace[0] != 0){
    key = cJSON_CreateArray();
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "exceptionStackTrace", key);

    key = cJSON_CreateString( oobevent.exceptionMessage);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "exceptionStackTrace", key);
  }

  if(oobevent.request[0] != 0){
    maskSecrets(oobevent.request);
    key = cJSON_CreateString( oobevent.request);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "request", key);
  }

  if(connectionInfo.sqlstate[0] != 0 ){
    key = cJSON_CreateString( connectionInfo.sqlstate);
    if( ! key ) goto end;
    cJSON_AddItemToObject(vals, "sqlState", key);
  }

  if (root)
  {
    str = cJSON_Print(root);
    cJSON_Delete(root);
  }

  freeAll();
  return str;

end:
  freeAll();
  if(root) {
    cJSON_Delete(root);
  }
  return NULL;

}

void copyString(const char* src, char* dst, int dstlen)
{
  //Care has been taken that src always fits in dst.
  long long len = strlen(src);
  len = (dstlen-1 > len)? len:(dstlen-1);
  sb_strncpy(dst, dstlen, src, len);
  dst[len]=0;
  return;
}

void setOOBeventdata(enum OOBINFO id, const char* data, long num)
{
  switch(id)
  {
    case OOBEVENTNAME:
      copyString(data, oobevent.name, 256);
      break;
    case EXCPMSG:
      copyString(data, oobevent.exceptionMessage, 4096);
      break;
    case EXCPMSGTRC:
      copyString(data, oobevent.exceptionStackTrace, 4096);
      break;
    case REQUESTURL:
      copyString(data, oobevent.request, 1024);
      break;
    case RESPSTATUSCODE:
      copyString(data, oobevent.responsestatuscode, 64);
      break;
    case ERRORCODE:
      oobevent.errorCode = num ;
      break;
    case OOBSQLSTATE:
      copyString(data, connectionInfo.sqlstate, 64);
      break;
    case CTX_STR:
      copyString(data, connectionInfo.ctxStr, 4096);
      break;
    case URGENCY:
      oobevent.urgent = num;
      break;
    case OOBCABUNDLE:
      copyString(data, connectionInfo.cabundle, 512);
  }
}

void setoobConnectioninfo(const char* host,
    const char* port,
    const char* account,
    const char* user,
    const char* token,
    const char* authenticator,
    const char* dbName,
    const char* schema,
    const char* warehouse,
    const char* role,
    short ssl
    )
{
  copyString(host, connectionInfo.host, 512);
  setdeployment(host);
  copyString(port, connectionInfo.port, 10);
  copyString(account, connectionInfo.account, 256);
  copyString(user, connectionInfo.user, 256);
  copyString(token, connectionInfo.token, 1024);
  copyString(authenticator, connectionInfo.authenticator, 1024);
  copyString(dbName, connectionInfo.dbName, 256);
  copyString(schema, connectionInfo.schema, 256);
  copyString(warehouse, connectionInfo.warehouse, 256);
  copyString(role, connectionInfo.role, 256);
  if(ssl)
    copyString("https", connectionInfo.protocol, 8);
  else
    copyString("http", connectionInfo.protocol, 8);
}

void setOOBDsnInfo(KeyValuePair kvPair[], int num) {
    dsn = cJSON_CreateObject();
    for (int i = 0; i < num; ++i) {
        cJSON* val = cJSON_CreateString(kvPair[i].val);
        cJSON_AddItemToObject(dsn, kvPair[i].key, val);
        if (!strcasecmp(kvPair[i].key, "server")) {
            setdeployment(kvPair[i].val);
        }
    }
    return;
}

void setOOBSimbaInfo(KeyValuePair kvPair[], int num) {
    simba = cJSON_CreateObject();
    for (int i = 0; i < num; ++i) {
        cJSON* val = cJSON_CreateString(kvPair[i].val);
        cJSON_AddItemToObject(simba, kvPair[i].key, val);
    }
    return;
}

void setdeployment(const char *host)
{
  const char* tmp=NULL;
  if( !host || host[0] == 0) {
    sb_strcpy(connectionInfo.dep, sizeof(connectionInfo.dep), "Ignore");
    return;
  }

  if( (strstr(host, "local") != NULL) || (strstr(host, "reg") != NULL) ){
    sb_strcpy(connectionInfo.dep, sizeof(connectionInfo.dep), "dev");
  }

  else if( (strstr(host, "qa1") != NULL) || (strstr(host, "preprod") != NULL)){
    sb_strcpy(connectionInfo.dep, sizeof(connectionInfo.dep), "qa1");
  }

  else 
    sb_strcpy(connectionInfo.dep, sizeof(connectionInfo.dep), "prod");
}

void setConnectionString(char const* connStr)
{
  if(connStr && connStr[0] != 0 )
  {
    sb_strcpy(connectionInfo.ctxStr, sizeof(connectionInfo.ctxStr), connStr);
  }
}

char* getConnectionString(void)
{
  return connectionInfo.ctxStr;
}

void gettime( char *buffer)
{

#if defined(__linux__) || defined(__APPLE__)
  time_t timer;
  struct tm* tm_info;

  time(&timer);
  tm_info = localtime(&timer);
  strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
#else /* Windows */
  SYSTEMTIME t;
  // Get the system time, which is expressed in UTC
  GetSystemTime(&t);
  sb_sprintf(buffer, TMP_BUF_LEN, "%04d-%02d-%02d %02d:%02d:%02d",
      t.wYear, t.wMonth, t.wDay,
      t.wHour, t.wMinute, t.wSecond);
#endif
  return;
}

void getCabundle(char *cabundle, int maxlen)
{
  sb_strncpy(cabundle, maxlen, connectionInfo.cabundle, sizeof(connectionInfo.cabundle));
  cabundle[maxlen-1]=0;
  return;
}

void getuuid(char* guid)
{
#ifdef _WIN32
  GUID out = {0} ;
  CoCreateGuid(&out);
  sb_sprintf(guid, TMP_BUF_LEN,"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", out.Data1, out.Data2, out.Data3, out.Data4[0], out.Data4[1], out.Data4[2], out.Data4[3], out.Data4[4], out.Data4[5], out.Data4[6], out.Data4[7]);
  for ( ; *guid; ++guid) *guid = tolower(*guid); // To lower one-liner
#else
  uuid_t binuuid;
  uuid_generate_random(binuuid);
  uuid_unparse_lower(binuuid, guid);
  return;
#endif
}

//Frees the logDetails
static void freeAll(void)
{
  memset(oobevent.name,0, 256);
  memset(connectionInfo.ctxStr,0,1024);
  memset(oobevent.exceptionMessage,0, 4096);
  memset(oobevent.exceptionStackTrace,0, 4096);
  memset(oobevent.request,0, 1024);
  memset(oobevent.responsestatuscode,0, 64);
  memset(connectionInfo.sqlstate,0, 64);
  oobevent.errorCode = 0;
  oobevent.urgent = 0;
  dsn = NULL;
  simba = NULL;
}
