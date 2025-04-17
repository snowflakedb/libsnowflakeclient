#ifndef SNOWFLAKECLIENT_SNOWFLAKECOMMON_HPP
#define SNOWFLAKECLIENT_SNOWFLAKECOMMON_HPP

#include <string>
#include <cstdint>
#include <type_traits>

/* CPP only utilities */
namespace Snowflake
{
namespace Client
{
namespace Util
{

void  replaceStrAll(std::string& stringToReplace,
                    std::string const& oldValue,
                    std::string const& newValue);

} // namespace Util
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_SNOWFLAKECOMMON_HPP
