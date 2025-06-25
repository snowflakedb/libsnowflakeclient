#include "../../lib/header_customizer_int.h"
#include "../logger/SFLogger.hpp"

// wrapper functions for C
extern "C" {

void _snowflake_curl_perform_callback(CURL* curl, void* userdata)
{
  // TODO: add implementation here
  // 1. Get information from curl (header, proxy header, url etc.)
  // 2. Call header customizer (userdata) with the information
  // 3. check customized headers with header protection
  // 4. set customized headers to curl
}

} // extern "C"

