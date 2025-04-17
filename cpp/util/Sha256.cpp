
/*
 * File: Sha256.cpp
 */

#include "Sha256.hpp"

#include <sstream>
#include <iomanip>
#include <memory>

#include "boost/optional.hpp"
#include "openssl/sha.h"
#include "openssl/types.h"
#include "openssl/evp.h"

namespace Snowflake {

  namespace Client {
    boost::optional<std::string> sha256(const std::string &str) {
      auto mdctx = std::unique_ptr<EVP_MD_CTX, std::function<void(EVP_MD_CTX *)>>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
      if (mdctx.get() == nullptr) {
        return {};
      }

      if (EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr) != 1) {
        return {};
      }

      if (EVP_DigestUpdate(mdctx.get(), str.c_str(), str.length()) != 1) {
        return {};
      }

      std::vector<unsigned char> buf(EVP_MD_size(EVP_sha256()));
      unsigned int size = 0;
      if (EVP_DigestFinal_ex(mdctx.get(), buf.data(), &size) != 1) {
        return {};
      }

      std::stringstream ss;
      for (short b: buf) {
        ss << std::hex << std::setw(2) << std::setfill('0') << b;
      }
      return ss.str();
    }
  }

}
