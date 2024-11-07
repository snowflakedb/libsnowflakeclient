#ifndef SNOWFLAKE_UTIL_H
#define SNOWFLAKE_UTIL_H

#include "picojson.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif
	typedef picojson::value jsonValue_t;
	typedef std::map<std::string, picojson::value> jsonObject_t;
	typedef std::vector<picojson::value> jsonArray_t;

	void cJSONtoPicoJson(cJSON* cjson, jsonObject_t& picojson);
	void picoJsonTocJson(jsonObject_t &picojson, cJSON** cjson);
	void strToPicoJson(jsonObject_t& picojson, std::string str);

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_UTIL_H