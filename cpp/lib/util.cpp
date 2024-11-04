#include "../../lib/util.h"

namespace Snowflake
{
namespace Client
{
	// wrapper functions for C
	extern "C" {
		void cJSONtoPicoJson(cJSON* cjson, jsonObject_t& picojson)
		{
			jsonValue_t v;
			const char* dataStr = snowflake_cJSON_Print(cjson);
			picojson::parse(v, dataStr);
			picojson = v.get<picojson::object>();
		}

		void picoJsonTocJson(jsonObject_t& picojson, cJSON** cjson)
		{
			std::string body_str = picojson::value(picojson).serialize();
			cJSON* new_body = snowflake_cJSON_Parse(body_str.c_str());
			snowflake_cJSON_DeleteItemFromObject(*cjson, "data");
			snowflake_cJSON_AddItemToObject(*cjson, "data", new_body);
		}
	}
}
}