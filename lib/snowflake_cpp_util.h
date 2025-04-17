#ifndef SNOWFLAKE_CPP_UTIL_H
#define SNOWFLAKE_CPP_UTIL_H

#include "picojson.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif
	typedef picojson::value jsonValue_t;
	typedef std::map<std::string, picojson::value> jsonObject_t;
	typedef std::vector<picojson::value> jsonArray_t;

	/*
	 * Convert the cJSON to picoJSON
	 */
	void cJSONtoPicoJson(cJSON *cjson, jsonObject_t &picojson);

	/*
	 * Convert the picojson to cJSON
	 */
	void picoJsonTocJson(jsonObject_t &picojson, cJSON **cjson);

	/*
	 * Stringfy the picojson data.
	 */
	void strToPicoJson(jsonObject_t &picojson, std::string &str);

	/**
	 * Verify that if two urls has same prefix (protocl + host + port)
	 * @param url1
	 * @param url2
	 *
	 * @return true if same prefix otherwise false
	 */
	bool urlHasSamePrefix(std::string url1, std::string url2);

	// trim from end (in place)
	static inline void rtrim(std::string &s, char c)
	{
		s.erase(std::find_if(s.rbegin(), s.rend(), [c](char ch)
							 { return ch != c; })
					.base(),
				s.end());
	}

	// trim from beginning (in place)
	static inline void ltrim(std::string &s, char c)
	{
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [c](char ch)
										{ return ch != c; }));
	}

	// trim from both ends (in place)
	static inline void trim(std::string &s, char c)
	{
		ltrim(s, c);
		rtrim(s, c);
	}
#ifdef __cplusplus
}
#endif
#endif // SNOWFLAKE_CPP_UTIL_H