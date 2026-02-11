#ifndef SNOWFLAKECLIENT_SNOWFLAKECOMMON_HPP
#define SNOWFLAKECLIENT_SNOWFLAKECOMMON_HPP

#include <string>
#include <map>
#include <cstdint>
#include <type_traits>

/* CPP only utilities */
namespace Snowflake
{
namespace Client
{
namespace Util
{

void replaceStrAll(std::string& stringToReplace,
                   std::string const& oldValue,
                   std::string const& newValue);

void parseHttpRespHeaders(std::string const& headerString,
                          std::map<std::string, std::string>& headers);

} // namespace Util
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_SNOWFLAKECOMMON_HPP
