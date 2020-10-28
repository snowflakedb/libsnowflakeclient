/*
* Copyright (c) 2018-2020 Snowflake Computing
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sf_ocsp_telemetry_data.h"

SF_OTD *get_ocsp_telemetry_instance()
{
  return (SF_OTD *)calloc(1, sizeof(SF_OTD));
}

void sf_otd_set_event_type(const char *event_type, SF_OTD* ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  snprintf(ocsp_telemetry_data->event_type,OCSP_TELEMETRY_EVENT_MAX_LEN, event_type);
}

void sf_otd_set_event_sub_type(const char *event_sub_type, SF_OTD* ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  if (strlen(ocsp_telemetry_data->event_sub_type) > 0)
  {
    strncat(ocsp_telemetry_data->event_sub_type, " | ",
        OCSP_TELEMETRY_SUB_EVENT_MAX_LEN-strlen(ocsp_telemetry_data->event_sub_type));
    strncat(ocsp_telemetry_data->event_sub_type, event_sub_type,
            OCSP_TELEMETRY_SUB_EVENT_MAX_LEN-strlen(ocsp_telemetry_data->event_sub_type));
  }
  else
  {
    snprintf(ocsp_telemetry_data->event_sub_type,OCSP_TELEMETRY_SUB_EVENT_MAX_LEN, event_sub_type);
  }
}

void sf_otd_set_sfc_peer_host(const char *sfc_peer_host, SF_OTD* ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  snprintf(ocsp_telemetry_data->sfc_peer_host, OCSP_TELEMETRY_HOSTNAME_MAX_LEN, sfc_peer_host);
}

void sf_otd_set_certid(const char *certid, SF_OTD* ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  snprintf(ocsp_telemetry_data->cert_id,OCSP_TELEMETRY_CERTID_MAX_LEN, certid);
}

void sf_otd_set_ocsp_request(const char *ocsp_req_b64, SF_OTD* ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  snprintf(ocsp_telemetry_data->ocsp_req_b64,OCSP_TELEMETRY_REQUEST_MAX_LEN, ocsp_req_b64);
}

void sf_otd_set_ocsp_responder_url(const char *ocsp_responder_url, SF_OTD* ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  snprintf(ocsp_telemetry_data->ocsp_responder_url,OCSP_TELEMETRY_OCSP_URL_MAX_LEN, ocsp_responder_url);
}

void sf_otd_set_error_msg(const char *error_msg, SF_OTD* ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  snprintf(ocsp_telemetry_data->error_msg,OCSP_TELEMETRY_ERROR_MSG_MAX_LEN, error_msg);
}

void sf_otd_set_insecure_mode(const int insecure_mode, SF_OTD *ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  ocsp_telemetry_data->insecure_mode = insecure_mode;
}

void sf_otd_set_fail_open_mode(const int failopen_mode, SF_OTD *ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  ocsp_telemetry_data->failopen_mode = failopen_mode;
}

void sf_otd_set_cache_hit(const int cache_hit, SF_OTD *ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  ocsp_telemetry_data->cache_hit = cache_hit;
}

void sf_otd_set_cache_enabled(const int cache_enabled, SF_OTD *ocsp_telemetry_data)
{
  if (!ocsp_telemetry_data)
  {
    return;
  }

  ocsp_telemetry_data->cache_enabled = cache_enabled;
}
