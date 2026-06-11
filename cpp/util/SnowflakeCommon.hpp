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

/**
 * Decide whether a GET download file name is safe to use as a local
 * destination. A valid name is a single path component: it must not be empty,
 * must not be "." or "..", and must not contain a path separator, a NUL byte
 * or (on Windows) a drive separator. Callers should reject names for which
 * this returns true.
 *
 * @param fileName the local destination file name to validate.
 * @return true if the name is not a single safe path component, false
 *         otherwise.
 */
bool isUnsafeDownloadFileName(std::string const& fileName);

} // namespace Util
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_SNOWFLAKECOMMON_HPP
