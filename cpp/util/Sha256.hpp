
/*
 * File: Sha256.hpp
 */

#ifndef SNOWFLAKECLIENT_SHA256_HPP
#define SNOWFLAKECLIENT_SHA256_HPP

#include <string>
#include <vector>

#include <boost/optional.hpp>

namespace Snowflake {
  namespace Client {
    boost::optional<std::string> sha256(const std::string &input);
  }
}

#endif //SNOWFLAKECLIENT_SHA256_HPP
