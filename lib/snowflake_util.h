#ifndef SNOWFLAKE_UTIL_H
#define SNOWFLAKE_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/snowflake/basic_types.h"

/**
 * Validate str ends with the suffix
 */
sf_bool ends_with(char* str, char* suffix);

	// trim from end (in place)
	static inline void rtrim(std::string& s, char c)
	{
		s.erase(std::find_if(s.rbegin(), s.rend(), [c](char ch) {
			return ch != c;
			}).base(), s.end());
	}

	// trim from beginning (in place)
	static inline void ltrim(std::string& s, char c)
	{
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [c](char ch) {
			return ch != c;
			}));
	}

	// trim from both ends (in place)
	static inline void trim(std::string& s, char c)
	{
		ltrim(s, c);
		rtrim(s, c);
	}
#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_UTIL_H
