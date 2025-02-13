/*
 * Copyright (c) 2025 Snowflake Computing, Inc. All rights reserved.
 */


#include "../../lib/snowflake_cpp_util.h"
#include "../include/snowflake/SFURL.hpp"

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

		void strToPicoJson(jsonObject_t& picojson, std::string& str)
		{
			jsonValue_t v;
			picojson::parse(v, str);
			picojson = v.get<picojson::object>();
		}

		bool urlHasSamePrefix(std::string url1, std::string url2)
		{
			SFURL parsed_url1 = SFURL::parse(url1);
			SFURL parsed_url2 = SFURL::parse(url2);

			if (parsed_url1.port() == "" && parsed_url1.scheme() == "https")
			{
				parsed_url1.port("443");
			}

			if (parsed_url2.port() == "" && parsed_url2.scheme() == "https")
			{
				parsed_url2.port("443");
			}

			return parsed_url1.scheme() == parsed_url2.scheme() &&
				parsed_url1.host() == parsed_url2.host() &&
				parsed_url1.port() == parsed_url2.port();
		}
	}
} // namespace Client
} // namespace Snowflake
